//
// Created by fimak on 4/29/17.
//

#ifndef EX1V2_METRICSC_H
#define EX1V2_METRICSC_H

typedef void CTest;

#ifdef __cplusplus
extern "C" {
#endif

CTest * test_new(int i);
void test_testfunc(const CTest *t);
void test_delete(CTest *t);

#ifdef __cplusplus
}
#endif




#endif //EX1V2_METRICSC_H
