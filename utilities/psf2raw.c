#include <stdio.h>
#include <stdbool.h>

#define PSF2_MAGIC0     0x72
#define PSF2_MAGIC1     0xb5
#define PSF2_MAGIC2     0x4a
#define PSF2_MAGIC3     0x86

struct psf2_header {
    unsigned char magic[4];
    unsigned int version;
    unsigned int headersize;    /* offset of bitmaps in file */
    unsigned int flags;
    unsigned int length;        /* number of glyphs */
    unsigned int charsize;      /* number of bytes for each character */
    unsigned int height, width; /* max dimensions of glyphs */
    /* charsize = height * ((width + 7) / 8) */
};

#define PSF1_MAGIC0     0x36
#define PSF1_MAGIC1     0x04

struct psf1_header {
    unsigned char magic[2];     /* Magic number */
    unsigned char mode;         /* PSF font mode */
    unsigned char charsize;     /* Character size */
};

static bool parse_psf1(FILE *stream, unsigned int *charsize, unsigned int *count) {
    struct psf1_header hdr;

    fseek(stream, 0, SEEK_SET);
    fread(&hdr, sizeof(hdr), 1, stream);
    if (hdr.magic[0] != PSF1_MAGIC0 || hdr.magic[1] != PSF1_MAGIC1)
        return false;

    printf("unsigned char console_font[] = {\n");

    *charsize = hdr.charsize;
    *count = 256;
    return true;
}

static bool parse_psf2(FILE *stream, unsigned int *charsize, unsigned int *count) {
    struct psf2_header hdr;

    fseek(stream, 0, SEEK_SET);
    fread(&hdr, sizeof(hdr), 1, stream);
    if (hdr.magic[0] != PSF2_MAGIC0 || hdr.magic[1] != PSF2_MAGIC1 ||
        hdr.magic[2] != PSF2_MAGIC2 || hdr.magic[3] != PSF2_MAGIC3)
    {
        return false;
    }

    fseek(stream, hdr.headersize, SEEK_SET);
    printf("unsigned char console_font_%ux%u[] = {\n", hdr.width, hdr.height);

    *charsize = hdr.charsize;
    *count = hdr.length;
    return true;
}

int main(int argc, char **argv) {
    unsigned int i, j, charsize, count;
    struct psf2_header hdr2;
    struct psf1_header hdr1;
    unsigned char buf[64];
    FILE *stream;

    stream = fopen(argv[1], "r");
    if (!parse_psf1(stream, &charsize, &count)) {
        if (!parse_psf2(stream, &charsize, &count))
            return 1;
    }

    for (i = 0; i < count; i++) {
        fread(buf, charsize, 1, stream);
        printf("\t");
        for (j = 0; j < charsize; j++) {
            if ((j + 1) == charsize) {
                printf("0x%02x,\n", buf[j]);
            } else {
                printf("0x%02x, ", buf[j]);
            }
        }
    }

    printf("};\n");
    return 0;
}
