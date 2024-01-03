#include "SilentPprf.h"
#if defined(ENABLE_SILENTOT) || defined(ENABLE_SILENT_VOLE)

#include <cryptoTools/Common/Log.h>
#include <cryptoTools/Common/Aligned.h>
#include <cryptoTools/Common/Range.h>
#include <cryptoTools/Crypto/RandomOracle.h>
#include <libOTe/Tools/Tools.h>

namespace osuCrypto
{

    void SilentMultiPprfSender::setBase(span<const std::array<block, 2>> baseMessages)
    {
        if (baseOtCount() != static_cast<u64>(baseMessages.size()))
            throw RTE_LOC;

        mBaseOTs.resize(mPntCount, mDepth);
        for (u64 i = 0; i < static_cast<u64>(mBaseOTs.size()); ++i)
            mBaseOTs(i) = baseMessages[i];
    }

    void SilentMultiPprfReceiver::setBase(span<const block> baseMessages)
    {
        if (baseOtCount() != static_cast<u64>(baseMessages.size()))
            throw RTE_LOC;

        // The OTs are used in blocks of 8, so make sure that there is a whole
        // number of blocks.
        mBaseOTs.resize(roundUpTo(mPntCount, 8), mDepth);
        memcpy(mBaseOTs.data(), baseMessages.data(), baseMessages.size() * sizeof(block));
    }

    // This function copies the leaf values of the GGM tree
    // to the output location. There are two modes for this
    // function. If interleaved == false, then each tree is
    // copied to a different contiguous regions of the output.
    // If interleaved == true, then trees are interleaved such that ....
    // @lvl         - the GGM tree leafs.
    // @output      - the location that the GGM leafs should be written to.
    // @numTrees    - How many trees there are in total.
    // @tIdx        - the index of the first tree.
    // @oFormat     - do we interleave the output?
    // @mal         - ...
    void copyOut(
        span<AlignedArray<block, 8>> lvl,
        MatrixView<block> output,
        u64 totalTrees,
        u64 tIdx,
        PprfOutputFormat oFormat,
        std::function<void(u64 treeIdx, span<AlignedArray<block, 8>> lvl)>& callback)
    {
        if (oFormat == PprfOutputFormat::ByLeafIndex)
        {

            auto curSize = std::min<u64>(totalTrees - tIdx, 8);
            if (curSize == 8)
            {

                for (u64 i = 0; i < output.rows(); ++i)
                {
                    auto oi = output[i].subspan(tIdx, 8);
                    auto& ii = lvl[i];
                    oi[0] = ii[0];
                    oi[1] = ii[1];
                    oi[2] = ii[2];
                    oi[3] = ii[3];
                    oi[4] = ii[4];
                    oi[5] = ii[5];
                    oi[6] = ii[6];
                    oi[7] = ii[7];
                }
            }
            else
            {
                for (u64 i = 0; i < output.rows(); ++i)
                {
                    auto oi = output[i].subspan(tIdx, curSize);
                    auto& ii = lvl[i];
                    for (u64 j = 0; j < curSize; ++j)
                        oi[j] = ii[j];
                }
            }

        }
        else if (oFormat == PprfOutputFormat::ByTreeIndex)
        {

            auto curSize = std::min<u64>(totalTrees - tIdx, 8);
            if (curSize == 8)
            {
                for (u64 i = 0; i < output.cols(); ++i)
                {
                    auto& ii = lvl[i];
                    output(tIdx + 0, i) = ii[0];
                    output(tIdx + 1, i) = ii[1];
                    output(tIdx + 2, i) = ii[2];
                    output(tIdx + 3, i) = ii[3];
                    output(tIdx + 4, i) = ii[4];
                    output(tIdx + 5, i) = ii[5];
                    output(tIdx + 6, i) = ii[6];
                    output(tIdx + 7, i) = ii[7];
                }
            }
            else
            {
                for (u64 i = 0; i < output.cols(); ++i)
                {
                    auto& ii = lvl[i];
                    for (u64 j = 0; j < curSize; ++j)
                        output(tIdx + j, i) = ii[j];
                }
            }

        }
        else if (oFormat == PprfOutputFormat::Callback)
            callback(tIdx, lvl);
        else
            throw RTE_LOC;
    }

    u64 interleavedPoint(u64 point, u64 treeIdx, u64 totalTrees, u64 domain, PprfOutputFormat format)
    {
        switch (format)
        {
        case osuCrypto::PprfOutputFormat::Interleaved:
        case osuCrypto::PprfOutputFormat::Callback:
        {

            if (domain <= point)
                return ~u64(0);

            auto subTree = treeIdx % 8;
            auto forest = treeIdx / 8;

            return (forest * domain + point) * 8 + subTree;
        }
        break;
        default:
            throw RTE_LOC;
            break;
        }
        //auto totalTrees = points.size();

    }

    void interleavedPoints(span<u64> points, u64 domain, PprfOutputFormat format)
    {

        for (u64 i = 0; i < points.size(); ++i)
        {
            points[i] = interleavedPoint(points[i], i, points.size(), domain, format);
        }
    }

    u64 getActivePath(const span<u8>& choiceBits)
    {
        u64 point = 0;
        for (u64 i = 0; i < choiceBits.size(); ++i)
        {
            auto shift = choiceBits.size() - i - 1;

            point |= u64(1 ^ choiceBits[i]) << shift;
        }
        return point;
    }

    void SilentMultiPprfReceiver::getPoints(span<u64> points, PprfOutputFormat format)
    {

        switch (format)
        {
        case PprfOutputFormat::ByLeafIndex:
        case PprfOutputFormat::ByTreeIndex:

            memset(points.data(), 0, points.size() * sizeof(u64));
            for (u64 j = 0; j < mPntCount; ++j)
            {
                points[j] = getActivePath(mBaseChoices[j]);
            }

            break;
        case PprfOutputFormat::Interleaved:
        case PprfOutputFormat::Callback:

            if ((u64)points.size() != mPntCount)
                throw RTE_LOC;
            if (points.size() % 8)
                throw RTE_LOC;

            getPoints(points, PprfOutputFormat::ByLeafIndex);
            interleavedPoints(points, mDomain, format);

            break;
        default:
            throw RTE_LOC;
            break;
        }
    }


    BitVector SilentMultiPprfReceiver::sampleChoiceBits(u64 modulus, PprfOutputFormat format, PRNG& prng)
    {
        BitVector choices(mPntCount * mDepth);

        // The points are read in blocks of 8, so make sure that there is a
        // whole number of blocks.
        mBaseChoices.resize(roundUpTo(mPntCount, 8), mDepth);
        for (u64 i = 0; i < mPntCount; ++i)
        {
            u64 idx;
            switch (format)
            {
            case osuCrypto::PprfOutputFormat::ByLeafIndex:
            case osuCrypto::PprfOutputFormat::ByTreeIndex:
                do {
                    for (u64 j = 0; j < mDepth; ++j)
                        mBaseChoices(i, j) = prng.getBit();
                    idx = getActivePath(mBaseChoices[i]);
                } while (idx >= modulus);

                break;
            case osuCrypto::PprfOutputFormat::Interleaved:
            case osuCrypto::PprfOutputFormat::Callback:

                if (modulus > mPntCount * mDomain)
                    throw std::runtime_error("modulus too big. " LOCATION);
                if (modulus < mPntCount * mDomain / 2)
                    throw std::runtime_error("modulus too small. " LOCATION);

                // make sure that at least the first element of this tree
                // is within the modulus.
                idx = interleavedPoint(0, i, mPntCount, mDomain, format);
                if (idx >= modulus)
                    throw RTE_LOC;


                do {
                    for (u64 j = 0; j < mDepth; ++j)
                        mBaseChoices(i, j) = prng.getBit();
                    idx = getActivePath(mBaseChoices[i]);

                    idx = interleavedPoint(idx, i, mPntCount, mDomain, format);
                } while (idx >= modulus);


                break;
            default:
                throw RTE_LOC;
                break;
            }

        }

        for (u64 i = 0; i < mBaseChoices.size(); ++i)
        {
            choices[i] = mBaseChoices(i);
        }

        return choices;
    }

    void SilentMultiPprfReceiver::setChoiceBits(PprfOutputFormat format, BitVector choices)
    {
        // Make sure we're given the right number of OTs.
        if (choices.size() != baseOtCount())
            throw RTE_LOC;

        mBaseChoices.resize(roundUpTo(mPntCount, 8), mDepth);
        for (u64 i = 0; i < mPntCount; ++i)
        {
            for (u64 j = 0; j < mDepth; ++j)
                mBaseChoices(i, j) = choices[mDepth * i + j];

            switch (format)
            {
            case osuCrypto::PprfOutputFormat::ByLeafIndex:
            case osuCrypto::PprfOutputFormat::ByTreeIndex:
                if (getActivePath(mBaseChoices[i]) >= mDomain)
                    throw RTE_LOC;

                break;
            case osuCrypto::PprfOutputFormat::Interleaved:
            case osuCrypto::PprfOutputFormat::Callback:
            {
                auto idx = getActivePath(mBaseChoices[i]);
                auto idx2 = interleavedPoint(idx, i, mPntCount, mDomain, format);
                if(idx2 > mPntCount * mDomain)
                    throw std::runtime_error("the base ot choice bits index outside of the domain. see sampleChoiceBits(...). " LOCATION);
                break;
            }
            default:
                throw RTE_LOC;
                break;
            }
        }
    }

    namespace
    {
        // A public PRF/PRG that we will use for deriving the GGM tree.
        const std::array<AES, 2> gAes = []() {
            std::array<AES, 2> aes;
            aes[0].setKey(toBlock(3242342));
            aes[1].setKey(toBlock(8993849));
            return aes;
            }();
    }


    void SilentMultiPprfSender::expandOne(
        block aesSeed,
        u64 treeIdx,
        bool programActivePath,
        span<span<AlignedArray<block, 8>>> levels,
        span<std::array<std::array<block, 8>, 2>> encSums,
        span<std::array<block, 4>> lastOts)
    {
        // The number of real trees for this iteration.
        auto min = std::min<u64>(8, mPntCount - treeIdx);

        // the first level should be size 1, the root of the tree.
        // we will populate it with random seeds using aesSeed in counter mode
        // based on the tree index.
        assert(levels[0].size() == 1);
        mAesFixedKey.ecbEncCounterMode(aesSeed ^ block(treeIdx), levels[0][0]);

        assert(encSums.size() == mDepth - programActivePath);
        assert(encSums.size() < 24);

        // space for our sums of each level. Should always be less then 
        // 24 levels... If not increase the limit or make it a vector.
        std::array<std::array<std::array<block, 8>, 2>, 24> sums;
        memset(&sums, 0, sizeof(sums));

        // For each level perform the following.
        for (u64 d = 0; d < mDepth; ++d)
        {
            // The previous level of the GGM tree.
            auto level0 = levels[d];

            // The next level of theGGM tree that we are populating.
            auto level1 = levels[d + 1];

            // The total number of parents in this level.
            auto width = divCeil(mDomain, 1ull << (mDepth - d));

            // use the optimized approach for intern nodes of the tree
            if (d + 1 < mDepth && 0)
            {
                // For each child, populate the child by expanding the parent.
                for (u64 parentIdx = 0, childIdx = 0; parentIdx < width; ++parentIdx, childIdx += 2)
                {
                    // The value of the parent.
                    auto& parent = level0.data()[parentIdx];

                    auto& child0 = level1.data()[childIdx];
                    auto& child1 = level1.data()[childIdx + 1];
                    mAesFixedKey.ecbEncBlocks<8>(parent.data(), child1.data());

                    child0[0] = child1[0] ^ parent[0];
                    child0[1] = child1[1] ^ parent[1];
                    child0[2] = child1[2] ^ parent[2];
                    child0[3] = child1[3] ^ parent[3];
                    child0[4] = child1[4] ^ parent[4];
                    child0[5] = child1[5] ^ parent[5];
                    child0[6] = child1[6] ^ parent[6];
                    child0[7] = child1[7] ^ parent[7];

                    // Update the running sums for this level. We keep 
                    // a left and right totals for each level.
                    auto& sum = sums[d];
                    sum[0][0] = sum[0][0] ^ child0[0];
                    sum[0][1] = sum[0][1] ^ child0[1];
                    sum[0][2] = sum[0][2] ^ child0[2];
                    sum[0][3] = sum[0][3] ^ child0[3];
                    sum[0][4] = sum[0][4] ^ child0[4];
                    sum[0][5] = sum[0][5] ^ child0[5];
                    sum[0][6] = sum[0][6] ^ child0[6];
                    sum[0][7] = sum[0][7] ^ child0[7];

                    child1[0] = child1[0] + parent[0];
                    child1[1] = child1[1] + parent[1];
                    child1[2] = child1[2] + parent[2];
                    child1[3] = child1[3] + parent[3];
                    child1[4] = child1[4] + parent[4];
                    child1[5] = child1[5] + parent[5];
                    child1[6] = child1[6] + parent[6];
                    child1[7] = child1[7] + parent[7];

                    sum[1][0] = sum[1][0] ^ child1[0];
                    sum[1][1] = sum[1][1] ^ child1[1];
                    sum[1][2] = sum[1][2] ^ child1[2];
                    sum[1][3] = sum[1][3] ^ child1[3];
                    sum[1][4] = sum[1][4] ^ child1[4];
                    sum[1][5] = sum[1][5] ^ child1[5];
                    sum[1][6] = sum[1][6] ^ child1[6];
                    sum[1][7] = sum[1][7] ^ child1[7];

                }
            }
            else
            {
                // for the leaf nodes we need to hash both children.
                for (u64 parentIdx = 0, childIdx = 0; parentIdx < width; ++parentIdx)
                {
                    // The value of the parent.
                    auto& parent = level0.data()[parentIdx];

                    // The bit that indicates if we are on the left child (0)
                    // or on the right child (1).
                    for (u64 keep = 0; keep < 2; ++keep, ++childIdx)
                    {
                        // The child that we will write in this iteration.
                        auto& child = level1[childIdx];

                        // The sum that this child node belongs to.
                        auto& sum = sums[d][keep];

                        // Each parent is expanded into the left and right children 
                        // using a different AES fixed-key. Therefore our OWF is:
                        //
                        //    H(x) = (AES(k0, x) + x) || (AES(k1, x) + x);
                        //
                        // where each half defines one of the children.
                        gAes[keep].hashBlocks<8>(parent.data(), child.data());

                        // Update the running sums for this level. We keep 
                        // a left and right totals for each level.
                        sum[0] = sum[0] ^ child[0];
                        sum[1] = sum[1] ^ child[1];
                        sum[2] = sum[2] ^ child[2];
                        sum[3] = sum[3] ^ child[3];
                        sum[4] = sum[4] ^ child[4];
                        sum[5] = sum[5] ^ child[5];
                        sum[6] = sum[6] ^ child[6];
                        sum[7] = sum[7] ^ child[7];
                    }
                }
            }
        }

        // For all but the last level, mask the sums with the
        // OT strings and send them over. 
        for (u64 d = 0; d < mDepth - programActivePath; ++d)
        {
            for (u64 j = 0; j < min; ++j)
            {
                encSums[d][0][j] = sums[d][0][j] ^ mBaseOTs[treeIdx + j][d][0];
                encSums[d][1][j] = sums[d][1][j] ^ mBaseOTs[treeIdx + j][d][1];
            }
        }

        if (programActivePath)
        {
            // For the last level, we are going to do something special. 
            // The other party is currently missing both leaf children of 
            // the active parent. Since this is the last level, we want
            // the inactive child to just be the normal value but the 
            // active child should be the correct value XOR the delta.
            // This will be done by sending the sums and the sums plus 
            // delta and ensure that they can only decrypt the correct ones.
            auto d = mDepth - 1;
            assert(lastOts.size() == min);
            for (u64 j = 0; j < min; ++j)
            {
                // Construct the sums where we will allow the delta (mValue)
                // to either be on the left child or right child depending 
                // on which has the active path.
                lastOts[j][0] = sums[d][0][j];
                lastOts[j][1] = sums[d][1][j] ^ mValue[treeIdx + j];
                lastOts[j][2] = sums[d][1][j];
                lastOts[j][3] = sums[d][0][j] ^ mValue[treeIdx + j];

                // We are going to expand the 128 bit OT string
                // into a 256 bit OT string using AES.
                std::array<block, 4> masks, maskIn;
                maskIn[0] = mBaseOTs[treeIdx + j][d][0];
                maskIn[1] = mBaseOTs[treeIdx + j][d][0] ^ AllOneBlock;
                maskIn[2] = mBaseOTs[treeIdx + j][d][1];
                maskIn[3] = mBaseOTs[treeIdx + j][d][1] ^ AllOneBlock;
                mAesFixedKey.hashBlocks<4>(maskIn.data(), masks.data());

                // Add the OT masks to the sums and send them over.
                lastOts[j][0] = lastOts[j][0] ^ masks[0];
                lastOts[j][1] = lastOts[j][1] ^ masks[1];
                lastOts[j][2] = lastOts[j][2] ^ masks[2];
                lastOts[j][3] = lastOts[j][3] ^ masks[3];
            }
        }
    }

    void allocateExpandBuffer(
        u64 depth,
        u64 activeChildXorDelta,
        std::vector<block>& buff,
        span< std::array<std::array<block, 8>, 2>>& sums,
        span< std::array<block, 4>>& last)
    {

        using SumType = std::array<std::array<block, 8>, 2>;
        using LastType = std::array<block, 4>;
        u64 numSums = depth - activeChildXorDelta;
        u64 numLast = activeChildXorDelta * 8;
        u64 numBlocks = numSums * 16 + numLast * 4;
        buff.resize(numBlocks);
        sums = span<SumType>((SumType*)buff.data(), numSums);
        last = span<LastType>((LastType*)(sums.data() + sums.size()), numLast);

        void* sEnd = sums.data() + sums.size();
        void* lEnd = last.data() + last.size();
        void* end = buff.data() + buff.size();
        if (sEnd > end || lEnd > end)
            throw RTE_LOC;
    }

    void allocateExpandTree(
        u64 dpeth,
        TreeAllocator& alloc,
        span<AlignedArray<block, 8>>& tree,
        std::vector<span<AlignedArray<block, 8>>>& levels)
    {
        tree = alloc.get();
        assert((u64)tree.data() % 32 == 0);
        levels[0] = tree.subspan(0, 1);
        auto rem = tree.subspan(2);
        for (auto i : rng(1ull, dpeth))
        {
            levels[i] = rem.subspan(0, levels[i - 1].size() * 2);
            assert((u64)levels[i].data() % 32 == 0);
            rem = rem.subspan(levels[i].size());
        }
    }

    void validateExpandFormat(
        PprfOutputFormat oFormat,
        MatrixView<block> output,
        u64 domain,
        u64 pntCount
    )
    {

        if (oFormat == PprfOutputFormat::ByLeafIndex)
        {
            if (output.rows() != domain)
                throw RTE_LOC;

            if (output.cols() != pntCount)
                throw RTE_LOC;
        }
        else if (oFormat == PprfOutputFormat::ByTreeIndex)
        {
            if (output.cols() != domain)
                throw RTE_LOC;

            if (output.rows() != pntCount)
                throw RTE_LOC;
        }
        else if (oFormat == PprfOutputFormat::Interleaved)
        {
            if (output.cols() != 1)
                throw RTE_LOC;
            if (domain & 1)
                throw RTE_LOC;

            auto rows = output.rows();
            if (rows > (domain * pntCount) ||
                rows / 128 != (domain * pntCount) / 128)
                throw RTE_LOC;
            if (pntCount & 7)
                throw RTE_LOC;
        }
        else if (oFormat == PprfOutputFormat::Callback)
        {
            if (domain & 1)
                throw RTE_LOC;
            if (pntCount & 7)
                throw RTE_LOC;
        }
        else
        {
            throw RTE_LOC;
        }
    }


    task<> SilentMultiPprfSender::expand(
        Socket& chl,
        span<const block> value,
        block seed,
        MatrixView<block> output,
        PprfOutputFormat oFormat,
        bool activeChildXorDelta,
        u64 numThreads)
    {
        if (activeChildXorDelta)
            setValue(value);

        setTimePoint("SilentMultiPprfSender.start");

        validateExpandFormat(oFormat, output, mDomain, mPntCount);

        MC_BEGIN(task<>, this, numThreads, oFormat, output, seed, &chl, activeChildXorDelta,
            i = u64{},
            mTreeAllocDepth = u64{},
            tree = span<AlignedArray<block, 8>>{},
            levels = std::vector<span<AlignedArray<block, 8>>>{},
            buff = std::vector<block>{},
            sums = span<std::array<std::array<block, 8>, 2>>{},
            last = span<std::array<block, 4>>{}
        );

        //if (oFormat == PprfOutputFormat::Callback && numThreads > 1)
        //    throw RTE_LOC;

        mTreeAllocDepth = mDepth + (oFormat != PprfOutputFormat::Interleaved);
        mTreeAlloc.reserve(numThreads, (1ull << mTreeAllocDepth) + 2);
        setTimePoint("SilentMultiPprfSender.reserve");

        levels.resize(mDepth + 1);
        allocateExpandTree(mTreeAllocDepth, mTreeAlloc, tree, levels);

        for (i = 0; i < mPntCount; i += 8)
        {
            // for interleaved format, the last level of the tree 
            // is simply the output.
            if (oFormat == PprfOutputFormat::Interleaved)
            {
                auto b = (AlignedArray<block, 8>*)output.data();
                auto forest = i / 8;
                b += forest * mDomain;

                levels.back() = span<AlignedArray<block, 8>>(b, mDomain);
            }

            // allocate the send buffer and partition it.
            allocateExpandBuffer(mDepth, activeChildXorDelta, buff, sums, last);

            // exapnd the tree
            expandOne(seed, i, activeChildXorDelta, levels, sums, last);

            MC_AWAIT(chl.send(std::move(buff)));

            // if we aren't interleaved, we need to copy the 
            // last layer to the output.
            if (oFormat != PprfOutputFormat::Interleaved)
                copyOut(levels.back(), output, mPntCount, i, oFormat, mOutputFn);
        }

        mBaseOTs = {};
        mTreeAlloc.del(tree);
        mTreeAlloc.clear();

        setTimePoint("SilentMultiPprfSender.de-alloc");

        MC_END();
    }

    void SilentMultiPprfSender::setValue(span<const block> value)
    {
        mValue.resize(mPntCount);

        if (value.size() == 1)
        {
            std::fill(mValue.begin(), mValue.end(), value[0]);
        }
        else
        {
            if ((u64)value.size() != mPntCount)
                throw RTE_LOC;

            std::copy(value.begin(), value.end(), mValue.begin());
        }
    }

    void SilentMultiPprfSender::clear()
    {
        mBaseOTs.resize(0, 0);
        mDomain = 0;
        mDepth = 0;
        mPntCount = 0;
    }

    void SilentMultiPprfReceiver::expandOne(

        u64 treeIdx,
        bool programActivePath,
        span<span<AlignedArray<block, 8>>> levels,
        span<std::array<std::array<block, 8>, 2>> theirSums,
        span<std::array<block, 4>> lastOts)
    {
        // This thread will process 8 trees at a time. 

        // special case for the first level.
        auto l1 = levels[1];
        for (u64 i = 0; i < 8; ++i)
        {

            // For the non-active path, set the child of the root node
            // as the OT message XOR'ed with the correction sum.
            int notAi = mBaseChoices[i + treeIdx][0];
            l1[notAi][i] = mBaseOTs[i + treeIdx][0] ^ theirSums[0][notAi][i];
            l1[notAi ^ 1][i] = ZeroBlock;
        }

        // space for our sums of each level.
        std::array<std::array<block, 8>, 2> mySums;

        // For all other levels, expand the GGM tree and add in 
        // the correction along the active path. 
        for (u64 d = 1; d < mDepth; ++d)
        {
            // The already constructed level. Only missing the
            // GGM tree node value along the active path. 
            auto level0 = levels[d];

            // The next level that we want to construct. 
            auto level1 = levels[d + 1];

            // Zero out the previous sums.
            memset(mySums.data(), 0, sizeof(mySums));

            // We will iterate over each node on this level and
            // expand it into it's two children. Note that the
            // active node will also be expanded. Later we will just
            // overwrite whatever the value was. This is an optimization.
            auto width = divCeil(mDomain, 1ull << (mDepth - d));

            // for internal nodes we the optimized approach.
            if (d + 1 < mDepth && 0)
            {
                for (u64 parentIdx = 0, childIdx = 0; parentIdx < width; ++parentIdx)
                {
                    // The value of the parent.
                    auto parent = level0[parentIdx];

                    auto& child0 = level1.data()[childIdx];
                    auto& child1 = level1.data()[childIdx + 1];
                    mAesFixedKey.ecbEncBlocks<8>(parent.data(), child1.data());

                    child0[0] = child1[0] ^ parent[0];
                    child0[1] = child1[1] ^ parent[1];
                    child0[2] = child1[2] ^ parent[2];
                    child0[3] = child1[3] ^ parent[3];
                    child0[4] = child1[4] ^ parent[4];
                    child0[5] = child1[5] ^ parent[5];
                    child0[6] = child1[6] ^ parent[6];
                    child0[7] = child1[7] ^ parent[7];

                    // Update the running sums for this level. We keep 
                    // a left and right totals for each level. Note that
                    // we are actually XOR in the incorrect value of the 
                    // children of the active parent (assuming !DEBUG_PRINT_PPRF).
                    // This is ok since we will later XOR off these incorrect values.
                    mySums[0][0] = mySums[0][0] ^ child0[0];
                    mySums[0][1] = mySums[0][1] ^ child0[1];
                    mySums[0][2] = mySums[0][2] ^ child0[2];
                    mySums[0][3] = mySums[0][3] ^ child0[3];
                    mySums[0][4] = mySums[0][4] ^ child0[4];
                    mySums[0][5] = mySums[0][5] ^ child0[5];
                    mySums[0][6] = mySums[0][6] ^ child0[6];
                    mySums[0][7] = mySums[0][7] ^ child0[7];

                    child1[0] = child1[0] + parent[0];
                    child1[1] = child1[1] + parent[1];
                    child1[2] = child1[2] + parent[2];
                    child1[3] = child1[3] + parent[3];
                    child1[4] = child1[4] + parent[4];
                    child1[5] = child1[5] + parent[5];
                    child1[6] = child1[6] + parent[6];
                    child1[7] = child1[7] + parent[7];

                    mySums[1][0] = mySums[1][0] ^ child1[0];
                    mySums[1][1] = mySums[1][1] ^ child1[1];
                    mySums[1][2] = mySums[1][2] ^ child1[2];
                    mySums[1][3] = mySums[1][3] ^ child1[3];
                    mySums[1][4] = mySums[1][4] ^ child1[4];
                    mySums[1][5] = mySums[1][5] ^ child1[5];
                    mySums[1][6] = mySums[1][6] ^ child1[6];
                    mySums[1][7] = mySums[1][7] ^ child1[7];
                }
            }
            else
            {
                // for leaf nodes both children should be hashed.
                for (u64 parentIdx = 0, childIdx = 0; parentIdx < width; ++parentIdx)
                {
                    // The value of the parent.
                    auto parent = level0[parentIdx];

                    for (u64 keep = 0; keep < 2; ++keep, ++childIdx)
                    {
                        // The child that we will write in this iteration.
                        auto& child = level1[childIdx];

                        // Each parent is expanded into the left and right children 
                        // using a different AES fixed-key. Therefore our OWF is:
                        //
                        //    H(x) = (AES(k0, x) + x) || (AES(k1, x) + x);
                        //
                        // where each half defines one of the children.
                        gAes[keep].hashBlocks<8>(parent.data(), child.data());

                        // Update the running sums for this level. We keep 
                        // a left and right totals for each level. Note that
                        // we are actually XOR in the incorrect value of the 
                        // children of the active parent (assuming !DEBUG_PRINT_PPRF).
                        // This is ok since we will later XOR off these incorrect values.
                        auto& sum = mySums[keep];
                        sum[0] = sum[0] ^ child[0];
                        sum[1] = sum[1] ^ child[1];
                        sum[2] = sum[2] ^ child[2];
                        sum[3] = sum[3] ^ child[3];
                        sum[4] = sum[4] ^ child[4];
                        sum[5] = sum[5] ^ child[5];
                        sum[6] = sum[6] ^ child[6];
                        sum[7] = sum[7] ^ child[7];
                    }
                }
            }

            // For everything but the last level we have to
            // 1) fix our sums so they dont include the incorrect
            //    values that are the children of the active parent
            // 2) Update the non-active child of the active parent. 
            if (!programActivePath || d != mDepth - 1)
            {
                for (u64 i = 0; i < 8; ++i)
                {
                    // the index of the leaf node that is active.
                    auto leafIdx = mPoints[i + treeIdx];

                    // The index of the active child node.
                    auto activeChildIdx = leafIdx >> (mDepth - 1 - d);

                    // The index of the active child node sibling.
                    auto inactiveChildIdx = activeChildIdx ^ 1;

                    // The indicator as to the left or right child is inactive
                    auto notAi = inactiveChildIdx & 1;

                    auto& inactiveChild = level1[inactiveChildIdx][i];

                    // correct the sum value by XORing off the incorrect
                    auto correctSum =
                        inactiveChild ^
                        theirSums[d][notAi][i];

                    inactiveChild =
                        correctSum ^
                        mySums[notAi][i] ^
                        mBaseOTs[i + treeIdx][d];

                }
            }
        }

        // last level.
        auto level = levels[mDepth];
        if (programActivePath)
        {
            // Now processes the last level. This one is special 
            // because we must XOR in the correction value as 
            // before but we must also fixed the child value for 
            // the active child. To do this, we will receive 4 
            // values. Two for each case (left active or right active).
            //timer.setTimePoint("recv.recvLast");

            auto d = mDepth - 1;
            for (u64 j = 0; j < 8; ++j)
            {
                // The index of the child on the active path. 
                auto activeChildIdx = mPoints[j + treeIdx];

                // The index of the other (inactive) child.
                auto inactiveChildIdx = activeChildIdx ^ 1;

                // The indicator as to the left or right child is inactive
                auto notAi = inactiveChildIdx & 1;

                std::array<block, 2> masks, maskIn;

                // We are going to expand the 128 bit OT string
                // into a 256 bit OT string using AES.
                maskIn[0] = mBaseOTs[j + treeIdx][d];
                maskIn[1] = mBaseOTs[j + treeIdx][d] ^ AllOneBlock;
                mAesFixedKey.hashBlocks<2>(maskIn.data(), masks.data());

                // now get the chosen message OT strings by XORing
                // the expended (random) OT strings with the lastOts values.
                auto& ot0 = lastOts[j][2 * notAi + 0];
                auto& ot1 = lastOts[j][2 * notAi + 1];
                ot0 = ot0 ^ masks[0];
                ot1 = ot1 ^ masks[1];

                auto& inactiveChild = level[inactiveChildIdx][j];
                auto& activeChild = level[activeChildIdx][j];

                // Fix the sums we computed previously to not include the
                // incorrect child values. 
                auto inactiveSum = mySums[notAi][j] ^ inactiveChild;
                auto activeSum = mySums[notAi ^ 1][j] ^ activeChild;

                // Update the inactive and active child to have to correct 
                // value by XORing their full sum with out partial sum, which
                // gives us exactly the value we are missing.
                inactiveChild = ot0 ^ inactiveSum;
                activeChild = ot1 ^ activeSum;
            }
            // pprf.setTimePoint("SilentMultiPprfReceiver.last " + std::to_string(treeIdx));

            //timer.setTimePoint("recv.expandLast");
        }
        else
        {
            for (auto j : rng(std::min<u64>(8, mPntCount - treeIdx)))
            {
                // The index of the child on the active path.
                auto activeChildIdx = mPoints[j + treeIdx];
                level[activeChildIdx][j] = ZeroBlock;
            }
        }
    }

    task<> SilentMultiPprfReceiver::expand(
        Socket& chl,
        MatrixView<block> output,
        PprfOutputFormat oFormat,
        bool activeChildXorDelta,
        u64 _)
    {
        validateExpandFormat(oFormat, output, mDomain, mPntCount);

        MC_BEGIN(task<>, this, oFormat, output, &chl, activeChildXorDelta,
            i = u64{},
            mTreeAllocDepth = u64{},
            tree = span<AlignedArray<block, 8>>{},
            levels = std::vector<span<AlignedArray<block, 8>>>{},
            buff = std::vector<block>{},
            sums = span<std::array<std::array<block, 8>, 2>>{},
            last = span<std::array<block, 4>>{}
        );

        setTimePoint("SilentMultiPprfReceiver.start");
        mPoints.resize(roundUpTo(mPntCount, 8));
        getPoints(mPoints, PprfOutputFormat::ByLeafIndex);

        mTreeAllocDepth = mDepth + (oFormat != PprfOutputFormat::Interleaved);
        mTreeAlloc.reserve(1, (1ull << mTreeAllocDepth) + 2);
        setTimePoint("SilentMultiPprfSender.reserve");

        levels.resize(mDepth + 1);
        allocateExpandTree(mTreeAllocDepth, mTreeAlloc, tree, levels);

        for (i = 0; i < mPntCount; i += 8)
        {
            // for interleaved format, the last level of the tree 
            // is simply the output.
            if (oFormat == PprfOutputFormat::Interleaved)
            {
                auto b = (AlignedArray<block, 8>*)output.data();
                auto forest = i / 8;
                b += forest * mDomain;

                levels.back() = span<AlignedArray<block, 8>>(b, mDomain);
            }

            // allocate the send buffer and partition it.
            allocateExpandBuffer(mDepth, activeChildXorDelta, buff, sums, last);

            MC_AWAIT(chl.recv(buff));

            // exapnd the tree
            expandOne(i, activeChildXorDelta, levels, sums, last);

            // if we aren't interleaved, we need to copy the 
            // last layer to the output.
            if (oFormat != PprfOutputFormat::Interleaved)
                copyOut(levels.back(), output, mPntCount, i, oFormat, mOutputFn);
        }

        setTimePoint("SilentMultiPprfReceiver.join");

        mBaseOTs = {};
        mTreeAlloc.del(tree);
        mTreeAlloc.clear();

        setTimePoint("SilentMultiPprfReceiver.de-alloc");

        MC_END();
    }

}

#endif
