#include <ksyms.h>
#include <stddef.h>

const KSym_t *ksym_find(uint64_t addr)
{
    if (g_ksyms_count == 0)
        return NULL;

    int64_t lo = 0, hi = (int64_t)g_ksyms_count - 1, best = -1;

    while (lo <= hi) {
        int64_t mid = lo + (hi - lo) / 2;
        if (g_ksyms[mid].addr <= addr) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return (best >= 0) ? &g_ksyms[best] : NULL;
}