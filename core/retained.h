/* A bounded, explicit review. No suffix-only deletion or automatic cleanup. */
#ifdef BS_OPENSSL
#define RETAINED_MAX 128
#define RETAINED_DEPTH 16
typedef struct {
    BSRetainedItem item;
    BSFileStamp part, meta;
} BSRetainedRecord;
typedef struct {
    void *dir;
    char relative[BS_PATH_MAX + 1];
} BSRetainedLevel;
struct BSRetainedReview {
    char root[BS_PATH_MAX + 1], error[160];
    unsigned long serial, transfer_serial, hi, lo, disk_hi, disk_lo;
    FILE *items;
    BSRetainedLevel levels[RETAINED_DEPTH];
    int depth, phase, count, visited, limited, kept, removed, cursor;
};
static void retained_dirs(struct BSRetainedReview *r) {
    while (r->depth) {
        r->depth--;
        if (r->levels[r->depth].dir)
            bsp_dir_close(r->levels[r->depth].dir);
        r->levels[r->depth].dir = NULL;
    }
}
void bs_retained_close(BSClient *c) {
    if (c && c->retained) {
        retained_dirs(c->retained);
        if (c->retained->items)
            fclose(c->retained->items);
        free(c->retained);
        c->retained = NULL;
    }
}
int bs_retained_phase(const BSClient *c) { return c && c->retained ? c->retained->phase : 0; }
unsigned long bs_retained_serial(const BSClient *c) {
    return c && c->retained ? c->retained->serial : 0;
}
int bs_retained_count(const BSClient *c) { return c && c->retained ? c->retained->count : 0; }
static int retained_read(const BSClient *c, int index, BSRetainedRecord *item) {
    return c && c->retained && index >= 0 && index < c->retained->count &&
           !fseek(c->retained->items, (long)index * (long)sizeof *item, SEEK_SET) &&
           fread(item, sizeof *item, 1, c->retained->items) == 1;
}
int bs_retained_item(const BSClient *c, int index, BSRetainedItem *out) {
    BSRetainedRecord item;
    if (!out || !retained_read(c, index, &item))
        return 0;
    *out = item.item;
    return 1;
}
unsigned long bs_retained_scan(BSClient *c, const char *root) {
    struct BSRetainedReview *r;
    if (!c || c->status.active || c->queue_count || c->tree_scan) {
        if (c)
            set_error(c, BS_ERR_BUSY,
                      "Finish or clear queued transfers before reviewing partial files");
        return 0;
    }
    if (!root || strlen(root) > BS_PATH_MAX - 14 || bsp_path_kind(root) != 2) {
        set_error(c, BS_ERR_DESTINATION, "Choose an existing receive folder, not a link");
        return 0;
    }
    bs_retained_close(c);
    r = (struct BSRetainedReview *)calloc(1, sizeof *r);
    if (!r) {
        set_error(c, BS_ERR_INTERNAL, "Not enough memory for partial-file review");
        return 0;
    }
    c->retained = r;
    copystr(r->root, root, sizeof r->root);
    r->items = bsp_temp_file();
    r->levels[0].dir = bsp_dir_open(root);
    r->depth = 1;
    r->phase = 1;
    r->transfer_serial = c->transfer_serial;
    c->retained_counter = (c->retained_counter + 1) & 0xffffffffUL;
    if (!c->retained_counter)
        c->retained_counter = 1;
    r->serial = c->retained_counter;
    if (!r->items || !r->levels[0].dir) {
        bs_retained_close(c);
        set_error(c, BS_ERR_STORAGE, "Cannot read the receive folder or create a private review");
        return 0;
    }
    return r->serial;
}
static int retained_header(const unsigned char *header, int length, unsigned long bytes,
                           unsigned long *total) {
    int n, i;
    if (length < 47 || memcmp(header, "BSRSM01", 8))
        return 0;
    n = get16(header + 44);
    if (n < 1 || n > 255 || length != 46 + n || memchr(header + 46, 0, (size_t)n))
        return 0;
    for (i = 8; i < 40; i++)
        if (hexval((char)header[i]) < 0)
            return 0;
    {
        char name[256];
        memcpy(name, header + 46, (size_t)n);
        name[n] = 0;
        if (!bs_safe_relative(name) || name[n - 1] == '/')
            return 0;
    }
    *total = get32(header + 40);
    return *total <= BS_FILE_MAX && bytes <= *total;
}
int bs_retained_remove(BSClient *c, unsigned long serial) {
    struct BSRetainedReview *r = c ? c->retained : NULL;
    if (!r || r->phase != 2 || !r->count || r->serial != serial) {
        if (c)
            set_error(c, BS_ERR_INPUT, "Review the listed partial files before confirming cleanup");
        return 0;
    }
    if (c->status.active || c->queue_count || c->tree_scan ||
        r->transfer_serial != c->transfer_serial) {
        set_error(
            c, BS_ERR_BUSY,
            "Transfers changed during review; finish transfers and scan again before cleanup");
        return 0;
    }
    r->phase = 3;
    r->cursor = 0;
    r->kept = r->removed = 0;
    return 1;
}
int bs_retained_step(BSClient *c) {
    struct BSRetainedReview *r = c ? c->retained : NULL;
    int budget = 16;
    if (!r)
        return 0;
    if (r->phase != 1 && r->phase != 3)
        return r->phase;
    if (c->status.active || c->queue_count || c->tree_scan)
        return r->phase;
    if (r->transfer_serial != c->transfer_serial) {
        retained_dirs(r);
        r->phase = 5;
        copystr(r->error, "Transfers changed during review. Scan again before cleanup.",
                sizeof r->error);
        return r->phase;
    }
    if (r->phase == 3) {
        BSRetainedRecord item;
        if (!retained_read(c, r->cursor, &item)) {
            r->phase = 5;
            copystr(r->error, "Cannot read the reviewed list; no further cleanup performed.",
                    sizeof r->error);
            return r->phase;
        }
        item.item.removed = bsp_retained_unlink(r->root, item.item.path, &item.part, &item.meta);
        if (item.item.removed == 0)
            item.item.removed = -1;
        else if (item.item.removed < 0)
            item.item.removed = -2;
        if (item.item.removed == 1)
            r->removed++;
        else
            r->kept++;
        if (fseek(r->items, (long)r->cursor * (long)sizeof item, SEEK_SET) ||
            fwrite(&item, sizeof item, 1, r->items) != 1) {
            r->phase = 5;
            return r->phase;
        }
        r->cursor++;
        if (r->cursor >= r->count)
            r->phase = 4;
        return r->phase;
    }
    while (budget-- && r->depth) {
        char name[256], relative[BS_PATH_MAX + 1], path[BS_PATH_MAX + 1];
        BSRetainedLevel *level = &r->levels[r->depth - 1];
        int rc = bsp_dir_next(level->dir, name, sizeof name), kind;
        size_t n;
        if (rc < 0) {
            retained_dirs(r);
            r->phase = 5;
            copystr(r->error,
                    "A folder could not be read. No cleanup performed; choose a narrower receive "
                    "folder.",
                    sizeof r->error);
            return r->phase;
        }
        if (!rc) {
            bsp_dir_close(level->dir);
            level->dir = NULL;
            r->depth--;
            continue;
        }
        if (++r->visited > 4096 || r->count >= RETAINED_MAX) {
            r->limited = 1;
            retained_dirs(r);
            break;
        }
        if (strlen(level->relative) + strlen(name) + 2 > sizeof relative)
            continue;
        relative[0] = 0;
        if (level->relative[0]) {
            strcpy(relative, level->relative);
            strcat(relative, "/");
        }
        strcat(relative, name);
        if (strlen(r->root) + strlen(relative) + 2 > sizeof path)
            continue;
        strcpy(path, r->root);
        strcat(path, "/");
        strcat(path, relative);
        kind = bsp_path_kind(path);
        if (kind == 2) {
            if (r->depth >= RETAINED_DEPTH) {
                r->limited = 1;
                continue;
            }
            level = &r->levels[r->depth];
            level->dir = bsp_dir_open(path);
            if (level->dir) {
                copystr(level->relative, relative, sizeof level->relative);
                r->depth++;
            } else
                r->limited = 1;
            continue;
        }
        n = strlen(relative);
        if (kind == 1 && n > 9 && !strcmp(relative + n - 9, ".bsk-part")) {
            BSRetainedRecord item;
            unsigned char header[302];
            int length;
            memset(&item, 0, sizeof item);
            relative[n - 9] = 0;
            copystr(item.item.path, relative, sizeof item.item.path);
            if (bsp_retained_pair(r->root, relative, header, &length, &item.item.bytes, &item.part,
                                  &item.meta) &&
                retained_header(header, length, item.item.bytes, &item.item.total)) {
                if (fseek(r->items, 0, SEEK_END) || fwrite(&item, sizeof item, 1, r->items) != 1) {
                    r->phase = 5;
                    retained_dirs(r);
                    return r->phase;
                }
                r->count++;
                wide_add(&r->hi, &r->lo, 0, item.item.bytes + (unsigned long)length);
                wide_add(&r->disk_hi, &r->disk_lo, item.part.word[13], item.part.word[14]);
                wide_add(&r->disk_hi, &r->disk_lo, item.meta.word[13], item.meta.word[14]);
            }
        }
    }
    if (!r->depth) {
        if (fflush(r->items)) {
            r->phase = 5;
            return r->phase;
        }
        r->phase = 2;
    }
    return r->phase;
}
int bs_retained_summary(const BSClient *c, char *out, size_t capacity) {
    const struct BSRetainedReview *r = c ? c->retained : NULL;
    char text[620], bytes[24], disk[24];
    if (!out || !capacity)
        return 0;
    out[0] = 0;
    if (!r)
        return 0;
    wide_decimal(r->hi, r->lo, bytes);
    wide_decimal(r->disk_hi, r->disk_lo, disk);
    if (r->phase == 1)
        sprintf(text, "Reviewing receive folder... %d recognized partial files (%s bytes) so far.",
                r->count, bytes);
    else if (r->phase == 2)
        sprintf(text,
                "%d recognized partial files contain %s bytes, with about %s bytes allocated. "
                "Completed "
                "and unrecognized files are kept.%s",
                r->count, bytes, disk,
                r->limited
                    ? " Scan limit reached; only listed files can be removed. Other files are kept."
                    : "");
    else if (r->phase == 3)
        sprintf(text,
                "Removing reviewed partial files: %d / %d processed; %d changed, locked or "
                "incomplete items kept.",
                r->cursor, r->count, r->kept);
    else if (r->phase == 4)
        sprintf(text,
                "Cleanup finished: %d partial files removed; %d changed, locked or incomplete "
                "items kept. "
                "Completed files were kept.",
                r->removed, r->kept);
    else
        sprintf(text, "Review stopped. %.159s Completed files were kept.",
                r->error[0] ? r->error : "No further cleanup performed.");
    copystr(out, text, capacity);
    return 1;
}
#else
void bs_retained_close(BSClient *c) { (void)c; }
int bs_retained_phase(const BSClient *c) {
    (void)c;
    return 0;
}
unsigned long bs_retained_serial(const BSClient *c) {
    (void)c;
    return 0;
}
int bs_retained_count(const BSClient *c) {
    (void)c;
    return 0;
}
int bs_retained_item(const BSClient *c, int i, BSRetainedItem *out) {
    (void)c;
    (void)i;
    (void)out;
    return 0;
}
unsigned long bs_retained_scan(BSClient *c, const char *root) {
    (void)root;
    if (c)
        set_error(c, BS_ERR_INPUT, "Retained partial files require a modern client");
    return 0;
}
int bs_retained_step(BSClient *c) {
    (void)c;
    return 0;
}
int bs_retained_summary(const BSClient *c, char *out, size_t cap) {
    (void)c;
    if (out && cap)
        out[0] = 0;
    return 0;
}
int bs_retained_remove(BSClient *c, unsigned long serial) {
    (void)c;
    (void)serial;
    return 0;
}
#endif
