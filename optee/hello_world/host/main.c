/*
 * Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* To the the UUID (found the the TA's h-file(s)) */
#include <hello_world_ta.h>

//#include "../src/h264bsd_decoder.h"
//#include "../src/h264bsd_util.h"

static struct timespec t0, t1;

static long pg_get_current_time(struct timespec *ts)
{
	if (clock_gettime(CLOCK_MONOTONIC, ts) < 0) {
		perror("clock_gettime");
		exit(1);
	}
	return 0;
}

static uint64_t pg_timespec_diff_ns(struct timespec *start, struct timespec *end)
{
	uint64_t ns = 0;

	if (end->tv_nsec < start->tv_nsec) {
		ns += 1000000000 * (end->tv_sec - start->tv_sec - 1);
		ns += 1000000000 - start->tv_nsec + end->tv_nsec;
	} else {
		ns += 1000000000 * (end->tv_sec - start->tv_sec);
		ns += end->tv_nsec - start->tv_nsec;
	}

	printf("NANOSEC : %llu\n", ns);
	return ns;
}

static char* outputPath = NULL;
static char* comparePath = NULL;
static int repeatTest = 0;

void createContentBuffer(char* contentPath, char** pContentBuffer, size_t* pContentSize) {
  struct stat sb;
  if (stat(contentPath, &sb) == -1) {
    perror("stat failed");
    exit(1);
  }

  *pContentSize = sb.st_size;
  *pContentBuffer = (char*)malloc(*pContentSize);
}

void pg_h264_decode(char* contentBuffer, size_t contentSize){
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_HELLO_WORLD_UUID;
	TEEC_SharedMemory shm;
	uint8_t val[] = {1, 2, 3, 4};
	uint32_t err_origin;
		
	/* Initialize a context connecting us to the TEE */
	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

	//cypbest
//	char pub_key[256] = {0, };
//	TEEC_SpecifyVendor(&ctx, pub_key, 2048);

	/*
	 * Open a session to the "hello world" TA, the TA will print "hello
	 * world!" in the log when the session is created.
	 */
	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, err_origin);

	shm.buffer = contentBuffer;
	shm.size = contentSize;
	shm.flags = TEEC_MEM_INPUT;

	res = TEEC_RegisterSharedMemory(&ctx, &shm);
	if(res != TEEC_SUCCESS){
		errx(1, "TEEC_RegisterSharedMemory failed with code 0x%x", res);
	}

	/*
	 * Execute a function in the TA by invoking it, in this case
	 * we're incrementing a number.
	 *
	 * The value of command ID part and how the parameters are
	 * interpreted is part of the interface provided by the TA.
	 */

	/* Clear the TEEC_Operation struct */
	memset(&op, 0, sizeof(op));

	/*
	 * Prepare the argument. Pass a value in the first parameter,
	 * the remaining three parameters are unused.
	 */
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT, 
					TEEC_NONE, TEEC_NONE, TEEC_NONE);
	op.params[0].memref.parent = &shm;
	op.params[0].memref.size = contentSize;

	printf("buffer(x) : %llx\n", contentBuffer);
	printf("size(d) : %lld\n", contentSize);
	
	/*
	 * TA_HELLO_WORLD_CMD_INC_VALUE is the actual function in the TA to be
	 * called.
	 */
//	printf("Invoking TA to increment %d\n", op.params[0].value.a);
	res = TEEC_InvokeCommand(&sess, TA_PG_CMD_H264_DECODE, &op,
				 &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x",
			res, err_origin);
//	printf("TA incremented value to %d\n", op.params[0].value.a);

	/*
	 * We're done with the TA, close the session and
	 * destroy the context.
	 *
	 * The TA will print "Goodbye!" in the log when the
	 * session is closed.
	 */

	TEEC_CloseSession(&sess);

	TEEC_FinalizeContext(&ctx);
	
}


void loadContent(char* contentPath, char* contentBuffer, size_t contentSize) {
  FILE *input = fopen(contentPath, "r");
  if (input == NULL) {
    perror("open failed");
    exit(1);
  }

  off_t offset = 0;
  while (offset < contentSize) {
    offset += fread(contentBuffer + offset, sizeof(char), contentSize - offset, input);
  }

  fclose(input);
}

static FILE *outputFile = NULL;

void savePic(char* picData, int width, int height, int picNum) {
  if(outputFile == NULL) {
    outputFile = fopen(outputPath, "w");
    if (outputFile == NULL) {
      perror("output file open failed");
      exit(1);
    }
  }

  size_t picSize = width * height * 3 / 2;
  off_t offset = 0;
  while (offset < picSize) {
    offset += fwrite(picData + offset, sizeof(char), picSize - offset, outputFile);
  }
}

static FILE *compareFile = NULL;
static char* expectedData = NULL;
static int totalErrors = 0;

int comparePics(char* actualData, int width, int height, int picNum) {
  if(compareFile == NULL) {
    compareFile = fopen(comparePath, "r");
    if (compareFile == NULL) {
      perror("compare file open failed");
      exit(1);
    }
  }

  size_t picSize = width * height * 3 / 2;
  size_t uDataOffset = width * height;
  size_t vDataOffset = width * height * 5 / 4;

  if (!expectedData) expectedData = (char*)malloc(picSize);

  off_t offset = 0;
  while (offset < picSize) {
    offset += fread(expectedData + offset, sizeof(char), picSize - offset, compareFile);
  }

  int numErrors = 0;

  size_t yOffset = 0;
  size_t uvOffset = 0;
	int y;
	int x;

  char* yExpected = expectedData;
  char* uExpected = expectedData + uDataOffset;
  char* vExpected = expectedData + vDataOffset;

  char* yActual = actualData;
  char* uActual = actualData + uDataOffset;
  char* vActual = actualData + vDataOffset;

  for (y=0; y<height; ++y) {
    for (x=0; x<width; ++x) {
      int ySame = yActual[yOffset] == yExpected[yOffset];
      int uSame = uActual[uvOffset] == uExpected[uvOffset];
      int vSame = vActual[uvOffset] == vExpected[uvOffset];

      if(!ySame || !uSame || !vSame) {
        ++numErrors;
        if (numErrors <= 5) {
          printf(
            "Pixel (%d,%d) is different. Expected (%d,%d,%d) but saw (%d,%d,%d).\n",
            x, y,
            yExpected[yOffset], uExpected[uvOffset], vExpected[uvOffset],
            yActual[yOffset], uActual[uvOffset], vActual[uvOffset]);
        }

        if (numErrors == 6) printf("...\n");
      }

      ++yOffset;
      if(yOffset % 1) ++ uvOffset;
    }
  }

  if(numErrors > 0) printf("%d pixels are different on frame %d.\n\n", numErrors, picNum);
  return numErrors;
}


/*
void decodeContent (char* contentBuffer, size_t contentSize) {
  unsigned int status;
  storage_t dec;
//  status = pg_h264bsdInit(&dec, HANTRO_FALSE);
  status = h264bsdInit(&dec, HANTRO_FALSE);

  if (status != HANTRO_OK) {
    fprintf(stderr, "h264bsdInit failed\n");
    exit(1);
  }

  char* byteStrm = contentBuffer;
  unsigned int readBytes;
  unsigned int len = contentSize;
  int numPics = 0;
  char* pic;
  unsigned int picId, isIdrPic, numErrMbs;
  unsigned int top, left, width, height, croppingFlag;
  int totalErrors = 0;

  while (len > 0) {
    unsigned int result = h264bsdDecode(&dec, byteStrm, len, 0, &readBytes);
	printf("[%d]readBytes : %d\n", result, readBytes);
    len -= readBytes;
    byteStrm += readBytes;

    switch (result) {
      case H264BSD_PIC_RDY:
        pic = h264bsdNextOutputPicture(&dec, &picId, &isIdrPic, &numErrMbs);
        ++numPics;
        if (outputPath) savePic(pic, width, height, numPics);
        if (comparePath) totalErrors += comparePics(pic, width, height, numPics);
        break;
      case H264BSD_HDRS_RDY:
        h264bsdCroppingParams(&dec, &croppingFlag, &left, &width, &top, &height);
        if (!croppingFlag) {
          width = h264bsdPicWidth(&dec) * 16;
          height = h264bsdPicHeight(&dec) * 16;
        }

        char* cropped = croppingFlag ? "(cropped) " : "";
        printf("Decoded headers. Image size %s%dx%d.\n", cropped, width, height);
        break;
      case H264BSD_RDY:
        break;
      case H264BSD_ERROR:
        printf("Error\n");
        exit(1);
      case H264BSD_PARAM_SET_ERROR:
        printf("Param set error\n");
        exit(1);
    }
  }

  h264bsdShutdown(&dec);

  printf("Test file complete. %d pictures decoded.\n", numPics);
  if (comparePath) printf("%d errors found.\n", totalErrors);
}
*/

int main(int argc, char *argv[])
{
	decode_main(argc, argv);
	return 0;
}


int decode_main(int argc, char *argv[]) {
  int c;
  while ((c = getopt (argc, argv, "ro:c:")) != -1) {
    switch (c) {
      case 'o':
        outputPath = optarg;
        break;
      case 'c':
        comparePath = optarg;
        break;
      case 'r':
        repeatTest = 1;
        break;
      default:
        abort();
    }
  }

  if (argc < 2) {
    fprintf(stderr, "Usage: %s [-r] [-c <compare.yuv>] [-o <output.yuv>] <test_video.h264>\n", argv[0]);
    exit(1);
  }

  char *contentPath = argv[argc - 1];
  char* contentBuffer;
  size_t contentSize;
  createContentBuffer(contentPath, &contentBuffer, &contentSize);

  if (repeatTest) {
    while (1) {
      loadContent(contentPath, contentBuffer, contentSize);
//      decodeContent(contentBuffer, contentSize);
    }
  } else {
    loadContent(contentPath, contentBuffer, contentSize);
	pg_h264_decode(contentBuffer, contentSize);
//    decodeContent(contentBuffer, contentSize);
  }

  if(outputFile) fclose(outputFile);
  if(compareFile) fclose(compareFile);
}

