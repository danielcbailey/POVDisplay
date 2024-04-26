

typedef struct {
    unsigned char* group1Buf;
    unsigned char* group2Buf;
    unsigned char* group3Buf;
    unsigned char* group4Buf;

    int group1BufLength; // in bursts
    int group2BufLength;
    int group3BufLength;
    int group4BufLength;
} GroupBufferInfo;

void runFileReader(const char* filename);

// When called, marks previous buffer as free and returns the next buffer.
GroupBufferInfo getGroupBuffers();