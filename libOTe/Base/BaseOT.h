#pragma once
// © 2016 Peter Rindal.
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "SimplestOT.h"
#include "MasnyRindal.h"
#include "McRosRoyTwist.h"
#include "MasnyRindalKyber.h"
#include "McRosRoy.h"
#include "libOTe/Tools/Popf/EKEPopf.h"
#include "libOTe/Tools/Popf/FeistelMulRistPopf.h"
#include "MockOt.h"

namespace osuCrypto
{
#define LIBOTE_HAS_BASE_OT
#ifdef ENABLE_SIMPLESTOT_ASM
    using DefaultBaseOT = AsmSimplestOT;
#elif defined ENABLE_MRR_TWIST && defined ENABLE_SSE
    using DefaultBaseOT = McRosRoyTwist;
#elif defined ENABLE_MR
    using DefaultBaseOT = MasnyRindal;
#elif defined ENABLE_MRR
    using DefaultBaseOT = McRosRoy;
#elif defined ENABLE_MR_KYBER
    using DefaultBaseOT = MasnyRindalKyber;
#elif defined ENABLE_SIMPLESTOT
    using DefaultBaseOT = SimplestOT;
#elif defined ENABLE_MOCK_OT
    using DefaultBaseOT = INSECURE_MOCK_OT;
#else
#undef LIBOTE_HAS_BASE_OT
#endif
}
