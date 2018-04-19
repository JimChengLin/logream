#pragma once
#ifndef LOGREAM_PREFETCH_H
#define LOGREAM_PREFETCH_H

#define LOGREAM_PREFETCH(addr, rw, locality) __builtin_prefetch(addr, rw, locality)

#endif //LOGREAM_PREFETCH_H
