#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#ifdef TARGET_ANDROID
#define FB_PATH "/dev/graphics/fb0"
#define IMAGE_PATH  "/sdcard/screenshot.bmp"
#else
#define FB_PATH "/dev/fb0"
#define IMAGE_PATH  "./screenshot.bmp"
#endif
typedef struct {
    char cfType[2];
    int cfSize;
    int cfReserved;
    int cfoffBits;
}__attribute__((packed)) BITMAPFILEHEADER;

typedef struct
{
    int ciSize;
    int ciWidth;
    int ciHeight;
    int short ciPlanes;
    int short ciPerPixelBits;
    int ciCompress;
    int ciSizeImage;
    int ciXPixelsPerMeter;
    int ciYPixelsPerMeter;
    int ciClrUsed;
    int ciClrImportant;
}__attribute__((packed)) BITMAPINFOHEADER;

typedef struct
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
}__attribute__((packed)) PIXEL;

BITMAPFILEHEADER fileHeader;
BITMAPINFOHEADER infoHeader;


static char *fbp = 0;
static int xres = 0;
static int yres = 0;
static int bits_per_pixel = 0;

int showBitmap();
void showFileHeader(BITMAPFILEHEADER fileHeader){
    printf("FileHeaer: struct size %ld\n", sizeof(BITMAPFILEHEADER));
    printf("\t cfType = %s:\n", fileHeader.cfType);
    printf("\t cfSize= %d:\n", fileHeader.cfSize);
    printf("\t cfReserved = %d:\n", fileHeader.cfReserved);
    printf("\t cfoffBits= %d:\n", fileHeader.cfoffBits);

    printf("\t sizeof(char) %ld, sizeof(int) %ld, sizeof(int short) %ld, \n",
               sizeof(char),     sizeof(int),     sizeof(int short));
};

void showInfoHeader(BITMAPINFOHEADER infoHeader){
    printf("InfoHeader: struct size %ld\n", sizeof(BITMAPINFOHEADER));
    printf("\t ciSize = %d\n", infoHeader.ciSize);
    printf("\t ciWidth = %d\n", infoHeader.ciWidth);
    printf("\t ciHeight = %d\n", infoHeader.ciHeight);
    printf("\t ciPlanes = %d\n", infoHeader.ciPlanes);
    printf("\t ciPerPixelBits = %d\n", infoHeader.ciPerPixelBits);
    printf("\t ciCompress = %d\n", infoHeader.ciCompress);
    printf("\t ciSizeImage = %d\n", infoHeader.ciSizeImage);
    printf("\t ciXPixelsPerMeter = %d\n", infoHeader.ciXPixelsPerMeter);
    printf("\t ciYPixelsPerMeter = %d\n", infoHeader.ciYPixelsPerMeter);
};

int main()
{
    int fbfd = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize = 0;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;


    fbfd = open(FB_PATH, O_RDWR);
    if(fbfd < 0){ printf("Cannot open fb device\n");exit(1);}

    if( ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) ){
        printf("Cannot read fixed information\n");
        exit(2);
    }

    if( ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) ){
        printf("Cannot read variable information\n");
        exit(3);
    }

    xres = vinfo.xres;
    yres = vinfo.yres;
    bits_per_pixel = vinfo.bits_per_pixel;
    printf("screen info:\n");
    printf("\t xres : %d\n", xres);
    printf("\t yres : %d\n", yres);
    printf("\t bits_per_pixel: %d\n", bits_per_pixel);

    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

    printf("screensize = %ld byte\n", screensize);

    fbp = (char*) mmap(0, screensize, PROT_READ | PROT_WRITE,
                        MAP_SHARED, fbfd, 0);
    if( (int)fbp == -1){
        printf("Cannot map framebuffer device to memory \n");
        exit(4);
    }

    showBitmap();

    munmap(fbp, screensize);
    close(fbfd);
    return 0;
}

int showBitmap(){
    FILE *fp;
    int rc;
    int xLine, yLine;
    long int locationInScreen = 0;

    fp = fopen(IMAGE_PATH, "rb");
    if(fp == NULL){
        printf("Cannot open the file\n");
        return -1;
    }
    rc = fread(&fileHeader, sizeof(BITMAPFILEHEADER), 1, fp);
    if(rc != 1){
        printf("Cannot read file header\n");
        fclose(fp);
        exit(-2);
    }
    //showFileHeader(fileHeader);

    if( memcmp(fileHeader.cfType, "BM", 2) != 0){
        printf("It's not a bitmap file \n");
        fclose(fp);
        exit(-3);
    }

    rc = fread(&infoHeader, sizeof(BITMAPINFOHEADER), 1, fp);
    if( rc != 1){
        printf("Cannot read info header\n");
        fclose(fp);
        exit(-4);
    }
   // showInfoHeader(infoHeader);

    fseek(fp, fileHeader.cfoffBits, SEEK_SET);

    int bitmapFileSize = infoHeader.ciWidth * infoHeader.ciHeight * infoHeader.ciPerPixelBits / 8;
    char* bitmapBuffer = (char*)calloc(1, bitmapFileSize);
    if(bitmapBuffer == NULL){printf("Cannot alloc memory\n");}
    int ret = 0;
    if (fread(bitmapBuffer, 1, bitmapFileSize, fp) != bitmapFileSize){
        printf("Cannot read all data from bitmap\n");
        return -1;
    }

    bitmapBuffer += bitmapFileSize;

    xLine = yLine = 0;

    int bytesOfScreenPixel = bits_per_pixel / 8;
    int bitmapHeight = infoHeader.ciHeight;
    int bitmapWidth = infoHeader.ciWidth;
    int bytesOfLineInBitmap = bitmapWidth * 3;
    bitmapBuffer -= bytesOfLineInBitmap;
    for(yLine = 0; yLine < bitmapHeight; yLine++){
        for(xLine = 0; xLine < bitmapWidth; xLine++){
            locationInScreen = (yLine * xres + xLine) * bytesOfScreenPixel;
            *(fbp + locationInScreen + 0) = *(bitmapBuffer + 0);
            *(fbp + locationInScreen + 1) = *(bitmapBuffer + 1);
            *(fbp + locationInScreen + 2) = *(bitmapBuffer + 2);
            *(fbp + locationInScreen + 3) = 0xff;

            bitmapBuffer += 3;
        }
        bitmapBuffer -= 2 * bytesOfLineInBitmap;
    }
    fclose(fp);
    return 0;
}
