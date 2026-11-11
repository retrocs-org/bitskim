/* Conservative destination mapping shared with collection previews. */
static int receive_name(const char *wire,char out[256]) {
    char original[256];int unchanged=bs_to_local(wire,original,sizeof original);
    if(!bs_suggest_name(wire,0,out,256))strcpy(out,"received-file");
    return !unchanged || strcmp(original,out)!=0;
}
