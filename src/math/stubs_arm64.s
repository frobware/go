// Copyright 2014 The Go Authors.  All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// +build arm64

#include "textflag.h"

TEXT ·Asin(SB),NOSPLIT,$0
	BL ·asin(SB)

TEXT ·Acos(SB),NOSPLIT,$0
	BL ·acos(SB)

TEXT ·Atan2(SB),NOSPLIT,$0
	BL ·atan2(SB)

TEXT ·Atan(SB),NOSPLIT,$0
	BL ·atan(SB)

TEXT ·Dim(SB),NOSPLIT,$0
	BL ·dim(SB)

TEXT ·Min(SB),NOSPLIT,$0
	BL ·min(SB)

TEXT ·Max(SB),NOSPLIT,$0
	BL ·max(SB)

TEXT ·Exp2(SB),NOSPLIT,$0
	BL ·exp2(SB)

TEXT ·Expm1(SB),NOSPLIT,$0
	BL ·expm1(SB)

TEXT ·Exp(SB),NOSPLIT,$0
	BL ·exp(SB)

TEXT ·Floor(SB),NOSPLIT,$0
	BL ·floor(SB)

TEXT ·Ceil(SB),NOSPLIT,$0
	BL ·ceil(SB)

TEXT ·Trunc(SB),NOSPLIT,$0
	BL ·trunc(SB)

TEXT ·Frexp(SB),NOSPLIT,$0
	BL ·frexp(SB)

TEXT ·Hypot(SB),NOSPLIT,$0
	BL ·hypot(SB)

TEXT ·Ldexp(SB),NOSPLIT,$0
	BL ·ldexp(SB)

TEXT ·Log10(SB),NOSPLIT,$0
	BL ·log10(SB)

TEXT ·Log2(SB),NOSPLIT,$0
	BL ·log2(SB)

TEXT ·Log1p(SB),NOSPLIT,$0
	BL ·log1p(SB)

TEXT ·Log(SB),NOSPLIT,$0
	BL ·log(SB)

TEXT ·Modf(SB),NOSPLIT,$0
	BL ·modf(SB)

TEXT ·Mod(SB),NOSPLIT,$0
	BL ·mod(SB)

TEXT ·Remainder(SB),NOSPLIT,$0
	BL ·remainder(SB)

TEXT ·Sincos(SB),NOSPLIT,$0
	BL ·sincos(SB)

TEXT ·Sin(SB),NOSPLIT,$0
	BL ·sin(SB)

TEXT ·Cos(SB),NOSPLIT,$0
	BL ·cos(SB)

TEXT ·Sqrt(SB),NOSPLIT,$0
	BL ·sqrt(SB)

TEXT ·Tan(SB),NOSPLIT,$0
	BL ·tan(SB)
