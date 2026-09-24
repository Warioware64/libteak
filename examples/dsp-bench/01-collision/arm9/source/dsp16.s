@ SPDX-License-Identifier: Zlib
@
@ Noyaux ARMv5TE (SMULxy / SMLAxy / SMLALxy) pour math_f16.h.
@
@ POURQUOI UN FICHIER .s SEPARE
@ -----------------------------
@ Les instructions SMULBB/SMLABB/SMLALBB n'existent qu'en mode ARM
@ (absentes du jeu Thumb-1 d'ARMv5TE). Le projet est compile en Thumb
@ par defaut (BlocksDS), donc ces noyaux vivent ici, dans un fichier
@ assemble avec ".arm" explicite : le mode est garanti quels que soient
@ les flags de compilation C.
@
@ NB : contrairement a ce qu'affirmait un commentaire precedent,
@ __attribute__((target("arm"))) N'EST PAS ignore par ce toolchain
@ (GCC 16). Il fonctionne, mais GCC accepte d'inliner une telle fonction
@ dans un appelant Thumb, ce qui recrache le SMLAxy en plein code Thumb
@ et fait echouer l'assemblage. C'est pourquoi math_f16.h impose
@ "noinline" sur MATH_F16_ARM_FN. Les deux voies (ce fichier, et les
@ intrinseques inline du header) sont donc valides et complementaires.
@
@ COUT D'INTERWORKING
@ -------------------
@ Appeler une de ces fonctions depuis du Thumb coute un BLX (changement
@ d'etat + vidage de pipeline) a l'aller et un BX LR au retour, en plus
@ du cout d'appel classique (pas d'inlining, arguments forces en
@ registres, pointeurs non propages). Pour un noyau de ~10 instructions
@ comme dot3, ce surcout DOMINE le calcul mesure.
@
@ C'est pourquoi ce fichier expose, en plus des primitives "une
@ operation par appel", des noyaux BATCH (dsp16_*_array) qui traitent N
@ elements par appel : le cout d'appel/interworking est alors amorti a
@ ~0 et le benchmark mesure bien l'arithmetique, pas la sequence
@ d'appel. Voir main.c.

    .arch   armv5te
    .syntax unified
    .arm

@ =====================================================================
@ Primitives brutes (1 operation par appel).
@ Utiles pour tester l'instruction elle-meme, PAS pour du code chaud :
@ le cout d'appel y est plusieurs fois superieur au calcul.
@ =====================================================================

@ ---------------------------------------------------------------------
@ int32_t dsp16_smulbb(int16_t a, int16_t b)
@ r0 = a, r1 = b (deja etendus en 32 bits par l'appelant, AAPCS).
@ SMULBB n'utilise de toute facon que les bits [15:0] de chaque source.
@ ---------------------------------------------------------------------
    .section .text.dsp16_smulbb, "ax", %progbits
    .align  2
    .global dsp16_smulbb
    .type   dsp16_smulbb, %function
dsp16_smulbb:
    smulbb  r0, r0, r1
    bx      lr
    .size   dsp16_smulbb, . - dsp16_smulbb

@ ---------------------------------------------------------------------
@ int32_t dsp16_smlabb(int16_t a, int16_t b, int32_t acc)
@ r0 = a, r1 = b, r2 = acc.
@ ---------------------------------------------------------------------
    .section .text.dsp16_smlabb, "ax", %progbits
    .align  2
    .global dsp16_smlabb
    .type   dsp16_smlabb, %function
dsp16_smlabb:
    smlabb  r0, r0, r1, r2
    bx      lr
    .size   dsp16_smlabb, . - dsp16_smlabb

@ =====================================================================
@ Operations vectorielles, 1 vecteur par appel.
@ =====================================================================

@ ---------------------------------------------------------------------
@ int32_t dsp16_dot3_fast(const vec3_f16 *a, const vec3_f16 *b)
@ r0 = a, r1 = b. Accumulateur 32 bits (voir l'avertissement overflow
@ dans math_f16.h).
@
@ Feuille pure : r2/r3/r12 sont caller-saved en AAPCS, aucun push.
@ ---------------------------------------------------------------------
    .section .text.dsp16_dot3_fast, "ax", %progbits
    .align  2
    .global dsp16_dot3_fast
    .type   dsp16_dot3_fast, %function
dsp16_dot3_fast:
    ldrsh   r2, [r0]            @ a.x
    ldrsh   r3, [r1]            @ b.x
    ldrsh   r12, [r0, #2]       @ a.y
    smulbb  r2, r2, r3          @ acc  = ax*bx
    ldrsh   r3, [r1, #2]        @ b.y
    smlabb  r2, r12, r3, r2     @ acc += ay*by
    ldrsh   r12, [r0, #4]       @ a.z
    ldrsh   r3, [r1, #4]        @ b.z
    smlabb  r0, r12, r3, r2     @ r0   = acc + az*bz
    bx      lr
    .size   dsp16_dot3_fast, . - dsp16_dot3_fast

@ ---------------------------------------------------------------------
@ int64_t dsp16_dot3_safe(const vec3_f16 *a, const vec3_f16 *b)
@ r0 = a, r1 = b. Retour AAPCS 64 bits : r0 = poids faible, r1 = fort.
@ Accumulation reelle sur 64 bits via SMLALBB -> aucun overflow
@ possible, quelle que soit la plage des entrees 16 bits.
@ ---------------------------------------------------------------------
    .section .text.dsp16_dot3_safe, "ax", %progbits
    .align  2
    .global dsp16_dot3_safe
    .type   dsp16_dot3_safe, %function
dsp16_dot3_safe:
    push    {r4, r5}            @ 2 registres = 8 octets : SP reste
                                @ aligne sur 8 (exigence AAPCS).

    ldrsh   r2, [r0]            @ a.x
    ldrsh   r3, [r1]            @ b.x
    ldrsh   r4, [r0, #2]        @ a.y
    ldrsh   r5, [r1, #2]        @ b.y
    smulbb  r12, r2, r3         @ lo = ax*bx (tient toujours sur 32 bits)

    ldrsh   r2, [r0, #4]        @ a.z
    ldrsh   r3, [r1, #4]        @ b.z
    asr     r0, r12, #31        @ hi = extension de signe de lo
                                @ (r0 libre : le pointeur a servi)

    smlalbb r12, r0, r4, r5     @ {hi,lo} += ay*by
    smlalbb r12, r0, r2, r3     @ {hi,lo} += az*bz

    mov     r1, r0              @ r1 = poids fort
    mov     r0, r12             @ r0 = poids faible
    pop     {r4, r5}
    bx      lr
    .size   dsp16_dot3_safe, . - dsp16_dot3_safe

@ ---------------------------------------------------------------------
@ void dsp16_cross_raw(const vec3_f16 *a, const vec3_f16 *b, int32_t *out)
@ r0 = a, r1 = b, r2 = out. Sortie brute (avant shift/clamp, faits cote
@ C) dans out[0..2] = {rx, ry, rz}.
@
@ CORRECTION IMPORTANTE vs la version precedente
@ ----------------------------------------------
@ L'ancienne version calculait A*B - C*D comme SMLABB(-C, D, A*B), en
@ negociant l'operande via "rsb r0, rC, #0" avant l'accumulation, avec
@ le commentaire "la negation se fait sur 32 bits donc pas de probleme
@ de -INT16_MIN". C'est FAUX : SMLABB ne lit que les bits [15:0] de ses
@ sources. Pour C = -32768 (0xFFFF8000), rsb donne 0x00008000, dont les
@ bits [15:0] valent 0x8000, re-etendus en -32768 par l'instruction.
@ Le terme etait donc ajoute au lieu d'etre soustrait : resultat faux,
@ silencieusement, des qu'une composante valait exactement INT16_MIN.
@
@ Ici on emet deux SMULBB puis un SUB 32 bits : la soustraction porte
@ sur les PRODUITS complets, jamais sur un operande 16 bits. Aucune
@ valeur d'entree n'est un cas particulier. Meme nombre d'instructions
@ (3 par composante), et une chaine de dependances plus courte que
@ rsb -> smlabb.
@ ---------------------------------------------------------------------
    .section .text.dsp16_cross_raw, "ax", %progbits
    .align  2
    .global dsp16_cross_raw
    .type   dsp16_cross_raw, %function
dsp16_cross_raw:
    push    {r4, r5}

    @ rx = ay*bz - az*by
    ldrsh   r3,  [r0, #2]       @ ay
    ldrsh   r12, [r1, #4]       @ bz
    ldrsh   r4,  [r0, #4]       @ az
    ldrsh   r5,  [r1, #2]       @ by
    smulbb  r3,  r3,  r12       @ ay*bz
    smulbb  r5,  r4,  r5        @ az*by
    sub     r3,  r3,  r5
    str     r3,  [r2]

    @ ry = az*bx - ax*bz        (r4 = az, r12 = bz encore valides)
    ldrsh   r5,  [r1]           @ bx
    ldrsh   r3,  [r0]           @ ax
    smulbb  r4,  r4,  r5        @ az*bx
    smulbb  r12, r3,  r12       @ ax*bz
    sub     r4,  r4,  r12
    str     r4,  [r2, #4]

    @ rz = ax*by - ay*bx        (r3 = ax, r5 = bx encore valides)
    ldrsh   r12, [r1, #2]       @ by
    ldrsh   r4,  [r0, #2]       @ ay
    smulbb  r12, r3,  r12       @ ax*by
    smulbb  r4,  r4,  r5        @ ay*bx
    sub     r12, r12, r4
    str     r12, [r2, #8]

    pop     {r4, r5}
    bx      lr
    .size   dsp16_cross_raw, . - dsp16_cross_raw

@ =====================================================================
@ Noyaux BATCH : N elements par appel.
@
@ Ce sont EUX qu'il faut mesurer face a du C. Un seul BLX/BX par passe
@ complete au lieu d'un par element : le cout d'interworking devient
@ negligeable et le chrono mesure l'arithmetique reelle.
@ Les pointeurs n'ont besoin que d'un alignement 2 (ldrsh), comme
@ n'importe quel tableau de vec3_f16.
@ =====================================================================

@ ---------------------------------------------------------------------
@ void dsp16_dot3_array(const vec3_f16 *a, const vec3_f16 *b,
@                       int32_t *out, uint32_t n)
@ out[i] = dot(a[i], b[i]), accumulateur 32 bits.
@ r0 = a, r1 = b, r2 = out, r3 = n. n == 0 est gere.
@ ---------------------------------------------------------------------
    .section .text.dsp16_dot3_array, "ax", %progbits
    @ Entree et boucle alignees sur une ligne de cache d'instructions
    @ (32 octets sur ARM946E-S), pour la meme raison que
    @ -falign-functions/-falign-loops cote C (voir le Makefile) : sans
    @ ca, la position de la boucle dans les lignes de cache depend de ce
    @ qui la precede dans le binaire, et les mesures ne sont plus
    @ comparables d'un build a l'autre.
    .balign 32
    .global dsp16_dot3_array
    .type   dsp16_dot3_array, %function
dsp16_dot3_array:
    cmp     r3, #0
    bxeq    lr
    push    {r4-r7}             @ 4 registres = 16 octets, SP aligne.

    @ Borne de fin plutot que compteur decroissant : libere r3 de la
    @ boucle et fusionne test de fin et avance du pointeur.
    add     r3, r3, r3, lsl #1  @ 3n
    add     r3, r0, r3, lsl #1  @ fin = a + 6n

    .balign 32
1:
    @ Ordonnancement : sur ARM9E le resultat d'un LDR n'est disponible
    @ qu'au cycle suivant, et celui d'un SMLAxy pas avant deux cycles.
    @ Les chargements de l'iteration sont donc entrelaces avec les MAC,
    @ de sorte qu'aucun operande ne soit consomme juste apres avoir ete
    @ produit. Charger les six halfwords d'un bloc en tete, comme le
    @ faisait la version precedente, serialisait les trois MAC et
    @ coutait ~1 tick par vecteur de plus que le code genere par GCC.
    ldrsh   r4, [r0]            @ a.x
    ldrsh   r5, [r1]            @ b.x
    ldrsh   r6, [r0, #2]        @ a.y
    ldrsh   r7, [r1, #2]        @ b.y
    smulbb  r12, r4, r5         @ acc  = ax*bx
    ldrsh   r4, [r0, #4]        @ a.z
    ldrsh   r5, [r1, #4]        @ b.z
    add     r0, r0, #6
    smlabb  r12, r6, r7, r12    @ acc += ay*by
    add     r1, r1, #6
    smlabb  r12, r4, r5, r12    @ acc += az*bz
    cmp     r0, r3
    str     r12, [r2], #4
    bne     1b

    pop     {r4-r7}
    bx      lr
    .size   dsp16_dot3_array, . - dsp16_dot3_array

@ ---------------------------------------------------------------------
@ void dsp16_cross_array(const vec3_f16 *a, const vec3_f16 *b,
@                        int32_t *out, uint32_t n)
@ out[3*i + 0..2] = cross(a[i], b[i]), brut (avant shift/clamp).
@ r0 = a, r1 = b, r2 = out, r3 = n. n == 0 est gere.
@ ---------------------------------------------------------------------
    .section .text.dsp16_cross_array, "ax", %progbits
    .balign 32
    .global dsp16_cross_array
    .type   dsp16_cross_array, %function
dsp16_cross_array:
    cmp     r3, #0
    bxeq    lr
    push    {r4-r10, lr}        @ 8 registres = 32 octets, SP aligne.

    add     r3, r3, r3, lsl #1  @ 3n
    add     r3, r0, r3, lsl #1  @ fin = a + 6n

    .balign 32
1:
    ldrsh   r4,  [r0]           @ ax
    ldrsh   r5,  [r0, #2]       @ ay
    ldrsh   r6,  [r0, #4]       @ az
    add     r0,  r0, #6
    ldrsh   r7,  [r1]           @ bx
    ldrsh   r8,  [r1, #2]       @ by
    ldrsh   lr,  [r1, #4]       @ bz
    add     r1,  r1, #6

    @ Les six produits sont emis en avance sur les trois soustractions,
    @ pour qu'aucun SUB ne lise un produit ecrit a l'instruction
    @ precedente (latence du multiplieur sur ARM9E).
    smulbb  r9,  r5, lr         @ ay*bz
    smulbb  r12, r6, r8         @ az*by
    smulbb  r10, r6, r7         @ az*bx
    sub     r9,  r9, r12        @ rx
    smulbb  r12, r4, lr         @ ax*bz
    smulbb  lr,  r5, r7         @ ay*bx  (bz n'est plus utile)
    sub     r10, r10, r12       @ ry
    smulbb  r12, r4, r8         @ ax*by
    cmp     r0,  r3
    sub     r12, r12, lr        @ rz

    @ STM range les registres par numero croissant : r9 -> [r2],
    @ r10 -> [r2+4], r12 -> [r2+8]. Un seul acces sequentiel au lieu de
    @ trois STR plus une avance de pointeur.
    stm     r2!, {r9, r10, r12}
    bne     1b

    pop     {r4-r10, pc}        @ LDM avec PC : interworking correct
                                @ sur ARMv5T (bit 0 choisit l'etat).
    .size   dsp16_cross_array, . - dsp16_cross_array

@ ---------------------------------------------------------------------
@ void dsp16_nop_call(void)
@ Feuille ARM vide. Sert UNIQUEMENT a chiffrer le cout d'un appel
@ Thumb -> ARM (BLX + BX LR + sequence d'appel) pour pouvoir le
@ retrancher mentalement des mesures "1 appel par element".
@ ---------------------------------------------------------------------
    .section .text.dsp16_nop_call, "ax", %progbits
    .align  2
    .global dsp16_nop_call
    .type   dsp16_nop_call, %function
dsp16_nop_call:
    bx      lr
    .size   dsp16_nop_call, . - dsp16_nop_call
