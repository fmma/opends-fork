/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef OPENDS_INTERNAL_H_
#define OPENDS_INTERNAL_H_

#include "opends.h"

static inline opends_error_t
opends_ok(void)
{
	return (opends_error_t){OPENDS_SUCCESS, 0};
}

static inline opends_error_t
opends_err(opends_op_error_t e)
{
	return (opends_error_t){e, 0};
}

#endif /* OPENDS_INTERNAL_H_ */
