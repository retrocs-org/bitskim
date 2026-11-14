#ifndef BS_WINDOW_FIT_H
#define BS_WINDOW_FIT_H
/* Work-area placement in physical pixels; preserves negative monitor origins. */
static RECT bs_window_fit(RECT work,int width,int height) {
    RECT fit;int available_width=work.right-work.left,available_height=work.bottom-work.top;
    if(available_width<1)available_width=1;
    if(available_height<1)available_height=1;
    if(width>available_width)width=available_width;
    if(height>available_height)height=available_height;
    if(width<1)width=1;
    if(height<1)height=1;
    fit.left=work.left+(available_width-width)/2;
    fit.top=work.top+(available_height-height)/2;
    fit.right=fit.left+width;fit.bottom=fit.top+height;
    return fit;
}

#endif
