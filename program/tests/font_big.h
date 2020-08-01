struct font_struct
{
    int width, height, bytes_per_line;
    int offset_num, offset_capital, offset_lower;
    int index_hash, index_star;
};

unsigned char fontdata_24x24[12][72] ;
unsigned char fontdata_64x64[12][512];

extern struct font_struct font_vga_24x24, font_vga_64x64;