/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*************************************************************************
 * 1. Includes
 ************************************************************************/
#include <time.h>
#include <tee_client_api.h>
#include "adbg_int.h"

//#define PG_DISABLE

/*************************************************************************
 * 2. Definition of external constants and variables
 ************************************************************************/
#ifndef TAILQ_CONCAT
#define	TAILQ_CONCAT(head1, head2, field) do { \
	if (!TAILQ_EMPTY(head2)) { \
	*(head1)->tqh_last = (head2)->tqh_first; \
	(head2)->tqh_first->field.tqe_prev = (head1)->tqh_last; \
	(head1)->tqh_last = (head2)->tqh_last; \
	TAILQ_INIT((head2)); \
	} \
} while (/*CONSTCOND*/0)
#endif

/*************************************************************************
 * 3. File scope types, constants and variables
 ************************************************************************/

TAILQ_HEAD(ADBG_CaseHead, ADBG_Case);
typedef struct ADBG_CaseHead ADBG_CaseHead_t;

typedef struct ADBG_Runner {
	ADBG_Result_t Result;
	const ADBG_Suite_Definition_t *Suite_p;

	ADBG_CaseHead_t CasesList;
} ADBG_Runner_t;

/*************************************************************************
 * 4. Declaration of file local functions
 ************************************************************************/

static int ADBG_RunSuite(ADBG_Runner_t *Runner_p, int argc, char *argv[]);

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

//	printf("sec : %llu\t, millisec : %llu\n", (unsigned long long)(end->tv_sec - start->tv_sec), (unsigned long long)(end->tv_nsec-start->tv_nsec)*1000000);
	printf("NANOSEC : %llu\n", ns);
	return ns;
}




/*************************************************************************
 * 5. Definition of external functions
 ************************************************************************/
int Do_ADBG_RunSuite(
	const ADBG_Suite_Definition_t *Suite_p,
	int argc,
	char *argv[]
	)
{
	ADBG_Runner_t *Runner_p;

	Runner_p = calloc(1, sizeof(*Runner_p));
	if (Runner_p == NULL) {
		Do_ADBG_Log("calloc failed for Suite %s!",
			    Suite_p->SuiteID_p);
		return -1;
	}
	TAILQ_INIT(&Runner_p->CasesList);
	Runner_p->Suite_p = Suite_p;

	int ret = ADBG_RunSuite(Runner_p, argc, argv);
	free(Runner_p);
	return ret;
}

int Do_ADBG_AppendToSuite(
	ADBG_Suite_Definition_t *Dest_p,
	ADBG_Suite_Definition_t *Source_p
	)
{
	char *p;
	size_t size;

	/* Append name of 'Source_p' to name of 'Dest_p' */
	size = strlen(Source_p->SuiteID_p);
	if (Dest_p->SuiteID_p) {
		size += strlen(Dest_p->SuiteID_p);
		size += 1; /* '+' */
	}
	size += 1; /* '\0' */
	p = malloc(size);
	if (!p) {
		fprintf(stderr, "malloc failed\n");
		return -1;
	}
	if (Dest_p->SuiteID_p)
		snprintf(p, size, "%s+%s", Dest_p->SuiteID_p,
			 Source_p->SuiteID_p);
	else
		strncpy(p, Source_p->SuiteID_p, size);
	free((void *)Dest_p->SuiteID_p);
	Dest_p->SuiteID_p = p;

	TAILQ_CONCAT(&Dest_p->cases, &Source_p->cases, link);
	return 0;
}

/*************************************************************************
 * 6. Definitions of internal functions
 ************************************************************************/
static int ADBG_RunSuite(
	ADBG_Runner_t *Runner_p,
	int argc,
	char *argv[]
	)
{
	ADBG_Case_t *Case_p;
	size_t NumSkippedTestCases = 0;
	int failed_test = 0;
	struct adbg_case_def *case_def;

	Do_ADBG_Log("######################################################");
	Do_ADBG_Log("#");
	Do_ADBG_Log("# %s", Runner_p->Suite_p->SuiteID_p);
	Do_ADBG_Log("#");
	Do_ADBG_Log("######################################################");

	TAILQ_FOREACH(case_def, &Runner_p->Suite_p->cases, link) {
		if (argc > 0) {
			bool HaveMatch = false;
			int i;

			for (i = 0; i < argc; i++) {

				if (strstr(case_def->TestID_p, argv[i])) {
					HaveMatch = true;
					break;
				}
			}
			if (!HaveMatch) {
				NumSkippedTestCases++;
				continue;
			}
		}

		Case_p = ADBG_Case_New(case_def);
		if (Case_p == NULL) {
			Do_ADBG_Log("HEAP_ALLOC failed for Case %s!",
				    case_def->TestID_p);
			Runner_p->Result.AbortTestSuite = 1;
			break;
		}

		TAILQ_INSERT_TAIL(&Runner_p->CasesList, Case_p, Link);

		/* Start the parent test case */
		Do_ADBG_BeginSubCase(Case_p, "%s", case_def->Title_p);

	//dhkwon : measure elapsed time of each test
	pg_get_current_time(&t0);

#ifndef PG_DISABLE	
//	char pub_key[256] = {9, };
//	TEEC_SetSecureOS(&ctx, pub_key, 2048);
#endif
		case_def->Run_fp(Case_p);

#ifndef PG_DISABLE
//	TEEC_UnSetSecureOS(&ctx, pub_key, 2048);        
#endif
	
	pg_get_current_time(&t1);
	pg_timespec_diff_ns(&t0, &t1);

		/* End abondoned subcases */
		while (Case_p->CurrentSubCase_p != Case_p->FirstSubCase_p)
			Do_ADBG_EndSubCase(Case_p, NULL);

		/* End the parent test case */
		Do_ADBG_EndSubCase(Case_p, "%s", case_def->Title_p);

		/* Sum up the errors */
		Runner_p->Result.NumTests += Case_p->Result.NumTests +
					     Case_p->Result.NumSubTests;
		Runner_p->Result.NumFailedTests +=
			Case_p->Result.NumFailedTests +
			Case_p->Result.
			NumFailedSubTests;
		Runner_p->Result.NumSubCases++;
		if (Case_p->Result.NumFailedTests +
		    Case_p->Result.NumFailedSubTests > 0)
			Runner_p->Result.NumFailedSubCases++;

		Runner_p->Result.AbortTestSuite = Case_p->Result.AbortTestSuite;

		if (Runner_p->Result.AbortTestSuite) {
			Do_ADBG_Log("Test suite aborted by %s!",
				    case_def->TestID_p);
			break;
		}
	}

	Do_ADBG_Log("+-----------------------------------------------------");
	if (argc > 0) {
		int i;

		for (i = 0; i < argc; i++)
			Do_ADBG_Log(
				"Result of testsuite %s filtered by \"%s\":",
				Runner_p->Suite_p->SuiteID_p, argv[i]);
	} else {
		Do_ADBG_Log("Result of testsuite %s:",
			    Runner_p->Suite_p->SuiteID_p);
	}

	TAILQ_FOREACH(Case_p, &Runner_p->CasesList, Link) {
		ADBG_SubCase_Iterator_t Iterator;
		ADBG_SubCase_t *SubCase_p;

		ADBG_Case_IterateSubCase(Case_p, &Iterator);
		while ((SubCase_p = ADBG_Case_NextSubCase(&Iterator)) != NULL) {
			if (SubCase_p->Result.NumFailedTests +
			    SubCase_p->Result.NumFailedSubTests > 0) {
				if (SubCase_p->Result.FirstFailedFile_p !=
				    NULL) {
					Do_ADBG_Log(
						"%s FAILED first error at %s:%d",
						SubCase_p->TestID_p,
						SubCase_p->
							Result.FirstFailedFile_p,
						SubCase_p->
							Result.FirstFailedRow);
				} else {
					Do_ADBG_Log("%s FAILED",
						    SubCase_p->TestID_p);
				}
			} else if (ADBG_Case_SubCaseIsMain(Case_p, SubCase_p)) {
				/* A level one test case is displayed
					if successfull too */
				Do_ADBG_Log("%s OK", SubCase_p->TestID_p);
			}
		}
	}


	Do_ADBG_Log("+-----------------------------------------------------");
	if (Runner_p->Result.AbortTestSuite)
		Do_ADBG_Log("Test suite was ABORTED");

	Do_ADBG_Log("%d subtest%s of which %d failed",
		    Runner_p->Result.NumTests,
		    Runner_p->Result.NumTests > 1 ? "s" : "",
		    Runner_p->Result.NumFailedTests);
	Do_ADBG_Log("%d test case%s of which %d failed",
		    Runner_p->Result.NumSubCases,
		    Runner_p->Result.NumSubCases > 1 ? "s" : "",
		    Runner_p->Result.NumFailedSubCases);
	Do_ADBG_Log("%zu test case%s was skipped",
		    NumSkippedTestCases,
		    NumSkippedTestCases > 1 ? "s" : "");

	failed_test = Runner_p->Result.NumFailedSubCases;

	while (true) {
		Case_p = TAILQ_FIRST(&Runner_p->CasesList);
		if (Case_p == NULL)
			break;
		TAILQ_REMOVE(&Runner_p->CasesList, Case_p, Link);
		ADBG_Case_Delete(Case_p);
	}
	return failed_test;
}
