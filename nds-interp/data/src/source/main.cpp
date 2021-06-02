#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <filesystem.h>

constexpr int WIDTH = 256, HEIGHT = 192;
constexpr int WIDTH2 = WIDTH / 2, HEIGHT2 = HEIGHT / 2;
constexpr int X_DIFF = 0x3000 / WIDTH2, Y_DIFF = 0x3000 / HEIGHT2;

constexpr int TEST_TOP_LEFT = 0;
constexpr int TEST_TOP_RIGHT = 1;
constexpr int TEST_BOTTOM_LEFT = 2;
constexpr int TEST_BOTTOM_RIGHT = 3;

// --- Configurable settings -----------------------------------------------------------------

// Select the dataset you wish to generate from one of the constants above
constexpr int TEST_TYPE = TEST_BOTTOM_RIGHT;

// Specify the bounding box of the area to be tested
constexpr u16 minX = 0;
constexpr u16 maxX = WIDTH;
constexpr u8 minY = 0;
constexpr u8 maxY = HEIGHT;

// Specify whether to take a screen capture of the last generated frame
constexpr bool screencap = false;

// Set to true to generate test data, or false to test/validate
constexpr bool generateData = true;

// -------------------------------------------------------------------------------------------

template <typename T>
T min(T x, T y) {
    return (x < y) ? x : y;
}

int16_t originalVerts[3][3] = {
    { -WIDTH2 * X_DIFF, HEIGHT2 * Y_DIFF, 0 },
    { -WIDTH2 * X_DIFF, HEIGHT2 * Y_DIFF, 0 },
    { -WIDTH2 * X_DIFF, HEIGHT2 * Y_DIFF, 0 },
};

int clip[16] = {
    0x1000, 0, 0, 0,
    0, 0x1000, 0, 0,
    0, 0, 0x1000, 0,
    0, 0, 0, 0x3000,
};
uint8_t colors[3][3] = {
    { 218, 165, 32 },
    { 112,128,144 },
    { 173,255,47 },
};

int16_t verts[3][3];

const char* cullStrs[3] = { "Front", "Back", "None" };

void test(PrintConsole* pc);
void generate(PrintConsole* pc);
void draw(u8 r, u8 g, u8 b);

int main() {
    PrintConsole* pc = consoleDemoInit();

    //set mode 0, enable BG0 and set it to 3D
    videoSetMode(MODE_0_3D);

    // initialize gl
    glInit();
    
    // setup the rear plane
    glClearColor(0,0,0,31); // BG must be opaque for AA to work
    glClearPolyID(63); // BG must have a unique polygon ID for AA to work
    glClearDepth(0x7FFF);

    //this should work the same as the normal gl call
    glViewport(0,0,255,191);
    
    m4x4 mat;
    memcpy(mat.m, clip, sizeof(clip));
    memcpy(verts, originalVerts, sizeof(originalVerts));

    glMatrixMode(GL_PROJECTION);
    glLoadMatrix4x4(&mat);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    int left = (TEST_TYPE & 1) ? WIDTH : 0;
    int top = (TEST_TYPE & 2) ? HEIGHT : 0;
    verts[0][0] += left * X_DIFF; verts[0][1] -= top * Y_DIFF;
    verts[1][0] += left * X_DIFF; verts[1][1] -= top * Y_DIFF;
    verts[2][0] += left * X_DIFF; verts[2][1] -= top * Y_DIFF;

    vramSetBankA(VRAM_A_LCD);
    REG_DISPCAPCNT = DCAP_MODE(DCAP_MODE_A)
                    | DCAP_SRC_A(DCAP_SRC_A_3DONLY)
                    | DCAP_SIZE(DCAP_SIZE_256x192)
                    | DCAP_OFFSET(0)
                    | DCAP_BANK(DCAP_BANK_VRAM_A);

    if (generateData) {
        generate(pc);
    } else {
        test(pc);
    }

    return 0;
}

void test(PrintConsole* pc) {
    nitroFSInit(NULL);
    FILE* file = fopen("data.bin", "rb");
    fseek(file, 0, SEEK_SET);

    char testType;
    fread(&testType, 1, sizeof(testType), file);

    u16 minX, maxX;
    u8 minY, maxY;
    fread(&minX, 1, sizeof(minX), file);
    fread(&maxX, 1, sizeof(maxX), file);
    fread(&minY, 1, sizeof(minY), file);
    fread(&maxY, 1, sizeof(maxY), file);

    glDisable(GL_ANTIALIAS);
    consoleClear();
    pc->cursorX = 0;
    pc->cursorY = 0;
    int prevX = 0, prevY = 0;
    for(int y = minY; y <= maxY; y++) {
        for(int x = minX; x <= maxX; x++) {
            swiWaitForVBlank();
            REG_DISPCAPCNT |= DCAP_ENABLE;
            verts[2][0] = originalVerts[2][0] + (x * X_DIFF);
            verts[2][1] = originalVerts[2][1] - (y * Y_DIFF);

            glPolyFmt(POLY_ALPHA(0) | POLY_CULL_NONE);
            draw(255, 255, 255);
            glFlush(0);

            while(REG_DISPCAPCNT & DCAP_ENABLE);

            u8 pos[2];
            fread(pos, 1, sizeof(pos), file);
            if(pos[0] != (u8)prevX || pos[1] != (u8)prevY) {
                printf("Invalid File\n");
                while(1);
            }
            
            int startY = (testType & 2) ? prevY : 0;
            int endY = (testType & 2) ? (HEIGHT - 1) : prevY;
            if (startY >= HEIGHT) startY = HEIGHT - 1;
            if (endY >= HEIGHT) endY = HEIGHT - 1;
            for(int checkY = startY; checkY <= endY; checkY++) {
                int startX = (testType & 1) ? prevX : 0;
                int endX = (testType & 1) ? (WIDTH - 1) : prevX;
                if (startX >= WIDTH) startX = WIDTH - 1;
                if (endX >= WIDTH) endX = WIDTH - 1;

                u8 first = 0, actualFirst;
                u8 last = endX, actualLast;
                bool found = false, actualFound;

                for(int checkX = startX; checkX <= endX; checkX++) {
                    if (!found) {
                        if(VRAM_A[checkY * WIDTH + checkX] & 0x7FFF) {
                            found = true;
                            first = checkX;
                        }
                    } else {
                        if ((VRAM_A[checkY * WIDTH + checkX] & 0x7FFF) == 0) {
                            last = checkX - 1;
                            break;
                        }
                    }
                }
                fread(&actualFound, 1, sizeof(actualFound), file);
                fread(&actualFirst, 1, sizeof(actualFirst), file);
                fread(&actualLast, 1, sizeof(actualLast), file);
                if(found != actualFound) {
                    if (found) printf("%ix%i Y=%i extra pixel\n", prevX, prevY, checkY);
                    if (actualFound) printf("%ix%i Y=%i missing pixel\n", prevX, prevY, checkY);
                }
                if(found && actualFound && (first != actualFirst || last != actualLast)) {
                    printf("%ix%i Y=%i %i-%i != %i-%i\n", prevX, prevY, checkY, first, last, actualFirst, actualLast);
                }
            }
            prevX = x;
            prevY = y;
        }
    }

    while(REG_DISPCAPCNT & DCAP_ENABLE);
    int startY = (testType & 2) ? prevY : 0;
    int endY = (testType & 2) ? (HEIGHT - 1) : prevY;
    if (startY >= HEIGHT) startY = HEIGHT - 1;
    if (endY >= HEIGHT) endY = HEIGHT - 1;
    for(int checkY = startY; checkY <= endY; checkY++) {
        int startX = (testType & 1) ? prevX : 0;
        int endX = (testType & 1) ? (WIDTH - 1) : prevX;
        if (startX >= WIDTH) startX = WIDTH - 1;
        if (endX >= WIDTH) endX = WIDTH - 1;

        u8 first = 0, actualFirst;
        u8 last = endX, actualLast;
        bool found = false, actualFound;

        for(int checkX = startX; checkX <= endX; checkX++) {
            if (!found) {
                if(VRAM_A[checkY * WIDTH + checkX] & 0x7FFF) {
                    found = true;
                    first = checkX;
                }
            } else {
                if ((VRAM_A[checkY * WIDTH + checkX] & 0x7FFF) == 0) {
                    last = checkX - 1;
                    break;
                }
            }
        }
        fread(&actualFound, 1, sizeof(actualFound), file);
        fread(&actualFirst, 1, sizeof(actualFirst), file);
        fread(&actualLast, 1, sizeof(actualLast), file);
        if(found != actualFound) {
            if (found) printf("%ix%i Y=%i extra pixel\n", prevX, prevY, checkY);
            if (actualFound) printf("%ix%i Y=%i missing pixel\n", prevX, prevY, checkY);
        }
        if(found && actualFound && (first != actualFirst || last != actualLast)) {
            printf("%ix%i Y=%i %i-%i != %i-%i\n", prevX, prevY, checkY, first, last, actualFirst, actualLast);
        }
    }
    fclose(file);

    while(1);
}

void generate(PrintConsole* pc) {
    fatInitDefault();
    const char *filename;
    switch (TEST_TYPE) {
        case TEST_TOP_LEFT: filename = "TL.bin"; break;
        case TEST_TOP_RIGHT: filename = "TR.bin"; break;
        case TEST_BOTTOM_LEFT: filename = "BL.bin"; break;
        case TEST_BOTTOM_RIGHT: filename = "BR.bin"; break;
        default: filename = "UNK.bin"; break;
    }
    FILE* file = fopen(filename, "wb");
    char testType = TEST_TYPE;
    fwrite(&testType, 1, sizeof(testType), file);

    fwrite(&minX, 1, sizeof(minX), file);
    fwrite(&maxX, 1, sizeof(maxX), file);
    fwrite(&minY, 1, sizeof(minY), file);
    fwrite(&maxY, 1, sizeof(maxY), file);

    auto countAndWrite = [&](int startX, int endX, int checkY) {
        u8 first = startX;
        u8 last = endX;
        bool found = false;
        for(int checkX = startX; checkX <= endX; checkX++) {
            u16 color = VRAM_A[checkY * WIDTH + checkX] & 0x7FFF;
            if (!found) {
                if (color != 0) {
                    found = true;
                    first = checkX;
                }
            } else {
                if (color == 0) {
                    last = checkX - 1;
                    break;
                }
            }
        }
        fwrite(&found, 1, sizeof(found), file);
        fwrite(&first, 1, sizeof(first), file);
        fwrite(&last, 1, sizeof(last), file);
    };

    auto drawFrame = [&](int x, int y) {
        swiWaitForVBlank();
        REG_DISPCAPCNT |= DCAP_ENABLE;
        pc->cursorX = 0;
        pc->cursorY = 0;
        consoleClear();
        printf("%i %i\n", x, y);

        verts[2][0] = originalVerts[2][0] + (x * X_DIFF);
        verts[2][1] = originalVerts[2][1] - (y * Y_DIFF);
        glPolyFmt(POLY_ALPHA(0) | POLY_CULL_NONE);
        draw(255, 255, 255);
        glFlush(0);

        while(REG_DISPCAPCNT & DCAP_ENABLE);
    };

    glDisable(GL_ANTIALIAS);
    glEnable(GL_BLEND);

    int prevX = 0, prevY = 0;
    for(int y = minY; y <= maxY; y++) {
        for(int x = minX; x <= maxX; x++) {
            drawFrame(x, y);

            u8 xCopy = prevX, yCopy = prevY;
            fwrite(&xCopy, 1, sizeof(xCopy), file); fwrite(&yCopy, 1, sizeof(yCopy), file);

            int startX = (TEST_TYPE & 1) ? min(prevX, WIDTH - 1) : 0;
            int endX = (TEST_TYPE & 1) ? (WIDTH - 1) : min(prevX, WIDTH - 1);
            int startY = (TEST_TYPE & 2) ? min(prevY, HEIGHT - 1) : 0;
            int endY = (TEST_TYPE & 2) ? (HEIGHT - 1) : min(prevY, HEIGHT - 1);
            for(int checkY = startY; checkY <= endY; checkY++) {
                countAndWrite(startX, endX, checkY);
            }
            prevX = x;
            prevY = y;
        }
    }

    drawFrame(maxX, maxY);

    if (screencap) {
        FILE* fileFS = fopen("linetest-screencap.bin", "wb");
        fseek(fileFS, 0, SEEK_SET);
        fwrite(VRAM_A, 256*192, sizeof(uint16_t), fileFS);
        fclose(fileFS);
    }

    int startX = (TEST_TYPE & 1) ? min(prevX, WIDTH - 1) : 0;
    int endX = (TEST_TYPE & 1) ? (WIDTH - 1) : min(prevX, WIDTH - 1);
    int startY = (TEST_TYPE & 2) ? min(prevY, HEIGHT - 1) : 0;
    int endY = (TEST_TYPE & 2) ? (HEIGHT - 1) : min(prevY, HEIGHT - 1);
    for(int checkY = startY; checkY <= endY; checkY++) {
        countAndWrite(startX, endX, checkY);
    }
    fclose(file);

    while(1) {
        pc->cursorX = 0;
        pc->cursorY = 0;
        printf("Done\n");
        swiWaitForVBlank();
    }
}

void draw(u8 r, u8 g, u8 b) {
    glBegin(GL_TRIANGLE);
        glColor3b(r, g, b);

        for(int16_t* vertex : verts) {
            glVertex3v16(vertex[0], vertex[1], vertex[2]);
        }
        
    glEnd();
}
