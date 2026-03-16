// nn_common.c
// Shared NN functions: ray casting, monster scan, feature building

#include <math.h>

#include "nn_common.h"
#include "doomstat.h"
#include "p_local.h"
#include "r_state.h"
#include "m_fixed.h"
#include "tables.h"

// ---------------------------------------------------------------
// Ray casting
// ---------------------------------------------------------------

static fixed_t ray_hit_dist;

static boolean NN_RayCallback(intercept_t *in)
{
    line_t *li;

    if (!in->isaline)
        return true;

    li = in->d.line;

    if (li->backsector)
    {
        sector_t *front = li->frontsector;
        sector_t *back  = li->backsector;
        fixed_t open_top    = (front->ceilingheight < back->ceilingheight)
                              ? front->ceilingheight : back->ceilingheight;
        fixed_t open_bottom = (front->floorheight > back->floorheight)
                              ? front->floorheight : back->floorheight;
        if (open_top - open_bottom >= 56 * FRACUNIT)
            return true;
    }

    ray_hit_dist = in->frac;
    return false;
}

float NN_CastRay(fixed_t x, fixed_t y, angle_t angle, fixed_t max_dist)
{
    fixed_t dx = FixedMul(max_dist, finecosine[angle >> ANGLETOFINESHIFT]);
    fixed_t dy = FixedMul(max_dist, finesine[angle >> ANGLETOFINESHIFT]);

    ray_hit_dist = FRACUNIT;
    P_PathTraverse(x, y, x + dx, y + dy, PT_ADDLINES, NN_RayCallback);

    float dist_frac = (float)ray_hit_dist / (float)FRACUNIT;
    float max_f     = (float)max_dist / (float)FRACUNIT;
    return dist_frac * max_f;
}

// ---------------------------------------------------------------
// Monster scanning (no line-of-sight check, simple & deterministic)
// ---------------------------------------------------------------

void NN_FindNearestMonsters(player_t *player, nn_monster_t *out, int max_n)
{
    thinker_t *th;
    mobj_t    *plmo = player->mo;

    static nn_monster_t all[256];
    int count = 0;

    float pangle = (float)plmo->angle * (2.0f * 3.14159265f / 4294967296.0f);
    float cos_a  = cosf(pangle);
    float sin_a  = sinf(pangle);
    float px = (float)plmo->x / (float)FRACUNIT;
    float py = (float)plmo->y / (float)FRACUNIT;

    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        mobj_t *mo;

        if (th->function.acp1 != (actionf_p1)P_MobjThinker)
            continue;

        mo = (mobj_t *)th;

        if (!(mo->flags & MF_COUNTKILL) || mo->health <= 0)
            continue;

        if (count >= 256)
            break;

        float mx = (float)mo->x / (float)FRACUNIT;
        float my = (float)mo->y / (float)FRACUNIT;
        float rdx = mx - px;
        float rdy = my - py;

        all[count].dx      =  rdx * cos_a + rdy * sin_a;
        all[count].dy      = -rdx * sin_a + rdy * cos_a;
        all[count].present = 1.0f;
        all[count].dist    = sqrtf(rdx * rdx + rdy * rdy);
        count++;
    }

    // Selection sort for N nearest
    for (int i = 0; i < max_n; i++)
    {
        if (i >= count)
        {
            out[i].dx      = 0.0f;
            out[i].dy      = 0.0f;
            out[i].present = 0.0f;
            out[i].dist    = 0.0f;
            continue;
        }

        int best = i;
        for (int j = i + 1; j < count; j++)
            if (all[j].dist < all[best].dist)
                best = j;

        if (best != i)
        {
            nn_monster_t tmp = all[i];
            all[i]    = all[best];
            all[best] = tmp;
        }
        out[i] = all[i];
    }
}

// ---------------------------------------------------------------
// Feature vector (35 floats)
// ---------------------------------------------------------------

void NN_BuildFeatures(player_t *player, const float *last_action, float *features)
{
    mobj_t *plmo = player->mo;
    int idx = 0;

    // Player state (5)
    features[idx++] = (float)plmo->angle / 4294967296.0f;
    features[idx++] = (float)player->health;
    features[idx++] = (float)plmo->momx / (float)FRACUNIT;
    features[idx++] = (float)plmo->momy / (float)FRACUNIT;
    features[idx++] = (float)player->readyweapon;

    // Rays (16)
    angle_t base = plmo->angle;
    angle_t step = ANG_MAX / NN_NUM_RAYS;
    for (int i = 0; i < NN_NUM_RAYS; i++)
    {
        features[idx++] = NN_CastRay(plmo->x, plmo->y,
                                     base + step * i,
                                     NN_RAY_MAX_DIST);
    }

    // Monsters (3 x 3 = 9): dx, dy, present
    nn_monster_t monsters[NN_MAX_MONSTERS];
    NN_FindNearestMonsters(player, monsters, NN_MAX_MONSTERS);
    for (int i = 0; i < NN_MAX_MONSTERS; i++)
    {
        features[idx++] = monsters[i].dx;
        features[idx++] = monsters[i].dy;
        features[idx++] = monsters[i].present;
    }

    // Last action (5): fwd_class, side_class, turn_class, fire, use
    for (int i = 0; i < 5; i++)
    {
        features[idx++] = last_action[i];
    }
}
