#pragma once

#include "butil/logging.h"

#ifndef GOOGLE_CHECK_NOTNULL
#define GOOGLE_CHECK_NOTNULL(val) CHECK((val) != NULL)
#endif
