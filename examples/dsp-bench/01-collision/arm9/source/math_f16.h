// SPDX-License-Identifier: Zlib
//
// Extension THEORIQUE / EXPERIMENTALE : dot/cross product en format
// fixed-point 16 bits generique ("f16"), analogue a f32 (nds/arm9/math.h)
// mais sur un container 16 bits au lieu de 32.
//
// IMPORTANT : contrairement a f32 dans libnds (ou 12 bits de fraction est
// une convention fixee par inttof32/f32toint), "f16" ici ne presuppose
// AUCUN nombre de bits fractionnaires particulier. Le parametre "shift"
// de crossf16() (et le reshift a faire toi-meme sur les dot products)
// est ce que tu choisis selon ton besoin (Q1.15, Q7.8, Q3.12, etc).
//
// ---------------------------------------------------------------------
// DEUX VOIES D'ACCES AUX INSTRUCTIONS DSP, ET POURQUOI
// ---------------------------------------------------------------------
// SMULBB/SMLABB/SMLALBB n'existent qu'en mode ARM (absentes de Thumb-1
// sur ARMv5TE). Ce header propose donc deux voies :
//
//  1. Les fonctions externes dsp16_* (dsp16.s) : appelables depuis
//     n'importe quel code, Thumb inclus. Le prix est un appel complet
//     + un changement d'etat Thumb->ARM (BLX) a l'aller et ARM->Thumb
//     (BX LR) au retour. Pour un noyau de 10 instructions, ce surcout
//     depasse le calcul lui-meme.
//
//  2. Les intrinseques inline dsp16i_* ci-dessous : zero appel, zero
//     interworking, le compilateur ordonnance et inline tout. Elles ne
//     sont utilisables QUE dans du code assemble en mode ARM (fichier
//     "xxx.arm.c", ou fonction marquee MATH_F16_ARM_FN).
//
// NOTE sur MATH_F16_ARM_FN et sur un commentaire errone des versions
// precedentes : __attribute__((target("arm"))) N'EST PAS ignore par ce
// toolchain (GCC 16), il fonctionne. Le piege est ailleurs : GCC accepte
// d'inliner une fonction target("arm") dans un appelant Thumb, et le
// SMLAxy atterrit alors en plein code Thumb -> erreur d'assemblage
// ("cannot honor width suffix"). D'ou le "noinline" joint a l'attribut.
// L'erreur est de toute facon detectee a la compilation, jamais
// silencieuse a l'execution.

#ifndef LIBNDS_NDS_ARM9_MATH_F16_H__
#define LIBNDS_NDS_ARM9_MATH_F16_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef int16_t f16;

typedef struct {
    f16 x, y, z;
} vec3_f16;

// ---------------------------------------------------------------------
// COMMENT OBTENIR DU CODE ARM DANS CE PROJET
//
// Voie recommandee : nommer le fichier "xxx.arm.c". Le Makefile
// BlocksDS a une regle dediee qui le compile avec -marm. Rien d'autre a
// faire : les intrinseques dsp16i_* s'y inlinent normalement, et GCC
// peut les ordonnancer avec le code autour. C'est ce que fait
// bench_arm.arm.c.
//
// Voie de secours : MATH_F16_ARM_FN, quand on ne peut pas isoler le
// code dans son propre fichier.
// - En TU Thumb : bascule la fonction en ARM et interdit son inlining
//   dans un appelant Thumb (sinon le SMLAxy fuirait en code Thumb et
//   l'assemblage echouerait).
// - En TU deja compilee en ARM (-marm) : macro vide, donc les noyaux
//   restent inlinables normalement.
//
// Dans les deux cas : mets le MAXIMUM de travail dans une seule
// fonction ARM (idealement une boucle complete sur N elements). Le cout
// d'interworking est alors paye une fois par appel, pas une fois par
// vecteur.
// ---------------------------------------------------------------------
#if defined(__thumb__)
#  define MATH_F16_ARM_FN __attribute__((target("arm"), noinline))
#else
#  define MATH_F16_ARM_FN
#endif

// Les instructions halfword-MAC existent a partir d'ARMv5TE.
#if defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5E__) \
    || (defined(__ARM_ARCH) && __ARM_ARCH >= 6)
#  define MATH_F16_HAVE_DSP 1
#endif

// ---------------------------------------------------------------------
// Implementees dans dsp16.s (mode ARM force par directive assembleur).
// Appelables depuis du Thumb, au prix d'un appel + interworking.
// ---------------------------------------------------------------------

/// SMULBB brut : Rd = a[15:0] * b[15:0] (signes).
extern int32_t dsp16_smulbb(int16_t a, int16_t b);

/// SMLABB brut : Rd = a[15:0] * b[15:0] + acc (signes).
extern int32_t dsp16_smlabb(int16_t a, int16_t b, int32_t acc);

/// Dot product 3D complet, UN SEUL appel (pas de surcout d'appels
/// repetes contrairement a une composition de dsp16_smulbb/smlabb).
extern int32_t dsp16_dot3_fast(const vec3_f16 *a, const vec3_f16 *b);

/// Dot product 3D via SMULBB + 2x SMLALBB, accumulateur 64 bits reel.
extern int64_t dsp16_dot3_safe(const vec3_f16 *a, const vec3_f16 *b);

/// Cross product 3D complet, UN SEUL appel. Sortie brute (avant shift
/// et clamp) dans out[0..2] = {rx, ry, rz}.
extern void dsp16_cross_raw(const vec3_f16 *a, const vec3_f16 *b, int32_t *out);

// --- Noyaux batch : N elements par appel, interworking amorti ---------

/// out[i] = dot(a[i], b[i]) pour i < n, accumulateur 32 bits.
extern void dsp16_dot3_array(const vec3_f16 *a, const vec3_f16 *b,
                             int32_t *out, uint32_t n);

/// out[3*i + 0..2] = cross(a[i], b[i]) brut, pour i < n.
extern void dsp16_cross_array(const vec3_f16 *a, const vec3_f16 *b,
                              int32_t *out, uint32_t n);

/// Feuille ARM vide : sert a mesurer le cout d'un appel Thumb -> ARM.
extern void dsp16_nop_call(void);

// ---------------------------------------------------------------------
// Intrinseques inline (mode ARM uniquement -> MATH_F16_ARM_FN).
//
// Les operandes sont pris en int32_t : SMULxy/SMLAxy ne lisent de toute
// facon que les bits [15:0], donc l'extension de signe faite par le
// compilateur en amont est libre, et passer par int32_t evite que GCC
// insere des sxth/lsl-asr inutiles.
// ---------------------------------------------------------------------
#ifdef MATH_F16_HAVE_DSP

/// acc = a[15:0] * b[15:0]
static inline int32_t dsp16i_smulbb(int32_t a, int32_t b)
{
    int32_t r;
    __asm__("smulbb %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

/// acc + a[15:0] * b[15:0], sur 32 bits (peut deborder, cf. dotf16_fast)
static inline int32_t dsp16i_smlabb(int32_t a, int32_t b, int32_t acc)
{
    int32_t r;
    __asm__("smlabb %0, %1, %2, %3" : "=r"(r) : "r"(a), "r"(b), "r"(acc));
    return r;
}

/// acc + a[15:0] * b[15:0], sur 64 bits (ne peut jamais deborder).
///
/// Les deux moities de l'accumulateur sont declarees "+&r" (early
/// clobber) : SMLALBB exige RdLo != RdHi, et l'early clobber garantit
/// en plus qu'aucune des sources ne partage un registre avec elles.
static inline int64_t dsp16i_smlalbb(int32_t a, int32_t b, int64_t acc)
{
    uint32_t lo = (uint32_t)acc;
    int32_t  hi = (int32_t)(acc >> 32);

    __asm__("smlalbb %0, %1, %2, %3"
            : "+&r"(lo), "+&r"(hi)
            : "r"(a), "r"(b));

    return (int64_t)(((uint64_t)(uint32_t)hi << 32) | lo);
}

#endif // MATH_F16_HAVE_DSP

// ---------------------------------------------------------------------
// Dot product - version RAPIDE (accumulateur 32 bits)
// ---------------------------------------------------------------------
//
// ATTENTION - overflow possible dans le pire cas :
// INT16_MIN * INT16_MIN = 2^30. Trois termes au pire cas simultanement
// = 3 * 2^30 > INT32_MAX (2^31-1). Cette version n'est SURE que si tu
// peux garantir que tes composantes ne saturent jamais leurs 16 bits
// simultanement sur les 3 axes (ce qui est une hypothese propre a TON
// usage, pas une propriete du format f16 en general).

/// Produit scalaire f16, accumulateur 32 bits (rapide, non protege
/// contre l'overflow dans le pire cas theorique).
///
/// Passe par un appel + interworking si l'appelant est en Thumb ; en
/// mode ARM, prefere dotf16_fast_inline().
///
/// @param a Vecteur f16.
/// @param b Vecteur f16.
/// @return  Somme brute des produits, echelle 2x le nombre de bits
///     fractionnaires d'entree (ex: si tes entrees sont en Q3.12,
///     sortie en Q6.24). A toi de reshifter selon ton besoin.
static inline int32_t dotf16_fast(const vec3_f16 *a, const vec3_f16 *b)
{
    return dsp16_dot3_fast(a, b);
}

// ---------------------------------------------------------------------
// Dot product - version SURE (accumulateur 64 bits)
// ---------------------------------------------------------------------
//
// Meme principe que normalizef32() dans le vrai libnds (qui utilise
// SMULL/SMLAL 32x32->64 pour ne jamais overflower), mais ici avec des
// halfwords : l'accumulation se fait dans une paire de registres 64
// bits, donc aucun overflow possible quelle que soit la plage des
// entrees 16 bits.

/// Produit scalaire f16, accumulateur 64 bits (sans risque d'overflow,
/// quelle que soit la plage des entrees 16 bits).
///
/// @param a Vecteur f16.
/// @param b Vecteur f16.
/// @return  Somme brute des produits sur 64 bits, echelle 2x le nombre
///     de bits fractionnaires d'entree.
static inline int64_t dotf16_safe(const vec3_f16 *a, const vec3_f16 *b)
{
    return dsp16_dot3_safe(a, b);
}

// ---------------------------------------------------------------------
// Cross product
// ---------------------------------------------------------------------
//
// ARMv5TE n'a pas de "multiply-subtract" halfword (pas de SMLSxy, qui
// n'existe qu'a partir d'ARMv6/SMLSD). Pour faire A*B - C*D on emet
// donc deux SMULBB suivis d'un SUB 32 bits.
//
// NE PAS essayer de remplacer le SUB par "SMLABB avec operande negue" :
// SMLABB ne lit que les bits [15:0] de ses sources, donc neguer -32768
// redonne 0x8000 = -32768 une fois tronque, et le terme est ajoute au
// lieu d'etre soustrait. C'etait le bug de la version precedente de
// dsp16_cross_raw ; la forme SMULBB+SUB n'a aucun cas particulier,
// INT16_MIN inclus, et coute le meme nombre d'instructions.

/// Applique le reshift puis la saturation a une composante brute.
static inline f16 f16_shift_clamp(int32_t raw, int shift)
{
    int32_t v = raw >> shift;

    if (v > INT16_MAX)
        return INT16_MAX;
    if (v < INT16_MIN)
        return INT16_MIN;

    return (f16)v;
}

/// Produit vectoriel f16, accumulateur 32 bits par composante.
///
/// @param a      Vecteur f16.
/// @param b      Vecteur f16.
/// @param result Vecteur f16 en sortie (apres reshift/clamp interne).
/// @param shift  Nombre de bits a decaler pour ramener le resultat
///     brut (echelle 2x fraction d'entree) a l'echelle f16 de sortie.
static inline void crossf16(const vec3_f16 *a, const vec3_f16 *b,
                            vec3_f16 *result, int shift)
{
    int32_t raw[3];

    dsp16_cross_raw(a, b, raw);

    result->x = f16_shift_clamp(raw[0], shift);
    result->y = f16_shift_clamp(raw[1], shift);
    result->z = f16_shift_clamp(raw[2], shift);
}

// ---------------------------------------------------------------------
// Variantes inline (appelant en mode ARM obligatoire).
//
// Meme semantique que ci-dessus, mais sans appel ni interworking : le
// compilateur voit tout le calcul et peut le fusionner avec le code
// environnant. Reserve aux fonctions marquees MATH_F16_ARM_FN.
// ---------------------------------------------------------------------
#ifdef MATH_F16_HAVE_DSP

static inline int32_t dotf16_fast_inline(const vec3_f16 *a, const vec3_f16 *b)
{
    int32_t acc = dsp16i_smulbb(a->x, b->x);

    acc = dsp16i_smlabb(a->y, b->y, acc);
    acc = dsp16i_smlabb(a->z, b->z, acc);

    return acc;
}

static inline int64_t dotf16_safe_inline(const vec3_f16 *a, const vec3_f16 *b)
{
    // ax*bx tient toujours sur 32 bits : on amorce l'accumulateur 64
    // bits avec ce produit etendu, puis on accumule en 64 bits reel.
    int64_t acc = dsp16i_smulbb(a->x, b->x);

    acc = dsp16i_smlalbb(a->y, b->y, acc);
    acc = dsp16i_smlalbb(a->z, b->z, acc);

    return acc;
}

/// Cross product inline : pas de tableau raw[3] intermediaire, donc pas
/// d'aller-retour par la pile comme dans crossf16().
static inline void crossf16_inline(const vec3_f16 *a, const vec3_f16 *b,
                                   vec3_f16 *result, int shift)
{
    int32_t ax = a->x, ay = a->y, az = a->z;
    int32_t bx = b->x, by = b->y, bz = b->z;

    int32_t rx = dsp16i_smulbb(ay, bz) - dsp16i_smulbb(az, by);
    int32_t ry = dsp16i_smulbb(az, bx) - dsp16i_smulbb(ax, bz);
    int32_t rz = dsp16i_smulbb(ax, by) - dsp16i_smulbb(ay, bx);

    result->x = f16_shift_clamp(rx, shift);
    result->y = f16_shift_clamp(ry, shift);
    result->z = f16_shift_clamp(rz, shift);
}

#endif // MATH_F16_HAVE_DSP

#ifdef __cplusplus
}
#endif

#endif // LIBNDS_NDS_ARM9_MATH_F16_H__
