#ifndef KW_UI_GRAPHICS_POLICY_H
#define KW_UI_GRAPHICS_POLICY_H
#include <stdint.h>
/* Stable persisted identities, deliberately distinct from slider ordinals. */
enum { KW_AA_OFF = 0, KW_AA_FXAA = 1, KW_AA_2 = 2, KW_AA_4 = 4, KW_AA_8 = 8 };
#define KW_AA_PROFILE_VERSION 0x100
struct kw_aa_modes {
    unsigned count, mode[5];
};
static inline struct kw_aa_modes kw_aa_modes_build(int fxaa, unsigned samples)
{
    struct kw_aa_modes m = {1, {KW_AA_OFF, 0, 0, 0, 0}};
    if (!fxaa)
        return m;
    m.mode[m.count++] = KW_AA_FXAA;
    for (unsigned n = 2; n <= 8; n *= 2)
        if (samples & (1U << n))
            m.mode[m.count++] = n;
    return m;
}
static inline unsigned kw_aa_mode_index(const struct kw_aa_modes *m, unsigned desired)
{
    unsigned result = 0;
    for (unsigned i = 1; i < m->count; ++i)
        if (m->mode[i] <= desired)
            result = i;
    return result;
}
static inline unsigned kw_aa_migrate(const struct kw_aa_modes *m, int legacy, int marker)
{
    unsigned desired;
    if (marker == KW_AA_PROFILE_VERSION + KW_AA_OFF || marker == KW_AA_PROFILE_VERSION + KW_AA_FXAA || marker == KW_AA_PROFILE_VERSION + KW_AA_2 ||
        marker == KW_AA_PROFILE_VERSION + KW_AA_4 || marker == KW_AA_PROFILE_VERSION + KW_AA_8)
        desired = (unsigned)(marker - KW_AA_PROFILE_VERSION);
    else
        desired = legacy <= 0 ? KW_AA_OFF : legacy < 4 ? KW_AA_FXAA : KW_AA_4;
    return kw_aa_mode_index(m, desired);
}
static inline int kw_aa_marker_valid(int marker)
{
    return marker == KW_AA_PROFILE_VERSION || marker == KW_AA_PROFILE_VERSION + 1 || marker == KW_AA_PROFILE_VERSION + 2 ||
           marker == KW_AA_PROFILE_VERSION + 4 || marker == KW_AA_PROFILE_VERSION + 8;
}
#endif
