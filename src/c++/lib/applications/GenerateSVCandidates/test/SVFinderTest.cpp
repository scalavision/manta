//
// Manta - Structural Variant and Indel Caller
// Copyright (c) 2013-2018 Illumina, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

/// \file
/// \author Atanu Pal
///

#include "boost/test/unit_test.hpp"
#include "manta/SVLocusEvidenceCount.hh"
#include "test/testAlignmentDataUtil.hh"
#include "test/testSVLocusScanner.hh"
#include "test/testUtil.hh"
#include "test/testFileMakers.hh"

#include "SVFinder.hh"
#include "SVFinder.cpp"

BOOST_AUTO_TEST_SUITE( SVFinderTest_test_suite )

// Test the fraction of anomalous or split evidence count to total evidence count
BOOST_AUTO_TEST_CASE( test_SpanningNoiseRate )
{
    AllSampleReadCounts counts;
    counts.setSampleCount(2);
    SampleReadCounts sample1(counts.getSampleCounts(0));
    sample1.input.evidenceCount.anom = 10;
    sample1.input.evidenceCount.split = 5;
    sample1.input.evidenceCount.anomAndSplit = 4;
    sample1.input.evidenceCount.total = 19;
    counts.getSampleCounts(0).merge(sample1);

    SampleReadCounts sample2(counts.getSampleCounts(1));
    sample2.input.evidenceCount.anom = 25;
    sample2.input.evidenceCount.split = 5;
    sample2.input.evidenceCount.anomAndSplit = 10;
    sample2.input.evidenceCount.total = 40;
    counts.getSampleCounts(1).merge(sample2);

    BOOST_REQUIRE_EQUAL(getSpanningNoiseRate(counts, 0), 0.020608439646712464);
    BOOST_REQUIRE_EQUAL(getSpanningNoiseRate(counts, 1), 0.028846153846153848);
}

// Test the fraction of semi-aligned evidence count to total evidence count
BOOST_AUTO_TEST_CASE( test_AssemblyNoiseRate )
{
    AllSampleReadCounts counts;
    counts.setSampleCount(2);
    SampleReadCounts sample1(counts.getSampleCounts(0));
    sample1.input.evidenceCount.assm = 10;
    sample1.input.evidenceCount.total = 19;
    counts.getSampleCounts(0).merge(sample1);

    SampleReadCounts sample2(counts.getSampleCounts(1));
    sample2.input.evidenceCount.assm = 25;
    sample2.input.evidenceCount.total = 40;
    counts.getSampleCounts(1).merge(sample2);

    BOOST_REQUIRE_EQUAL(getAssemblyNoiseRate(counts, 0), 0.019627085377821395);
    BOOST_REQUIRE_EQUAL(getAssemblyNoiseRate(counts, 1), 0.033653846153846152);
}

// test if read supports an SV on this edge, if so, add to SVData
BOOST_AUTO_TEST_CASE( test_AddSVNodeRead )
{
    const bam_header_info bamHeader(buildTestBamHeader());
    std::unique_ptr<SVLocusScanner> scanner(buildTestSVLocusScanner(bamHeader));
    SampleEvidenceCounts eCounts;

    const unsigned defaultReadGroupIndex(0);
    const reference_contig_segment refSeq;

    // supplementary read in SV evidence
    bam_record supplementSASplitRead;
    buildTestBamRecord(supplementSASplitRead);
    addSupplementaryAlignmentEvidence(supplementSASplitRead);

    // large insertion in SV evidence
    bam_record largeInsertionRead;
    buildTestBamRecord(largeInsertionRead, 0, 200, 0, 300, 100, 15, "100M2000I100M");
    largeInsertionRead.set_qname("large_insertion");

    SVLocus locus1;
    locus1.addNode(GenomeInterval(0,80,120));
    locus1.addNode(GenomeInterval(0,279,319));
    locus1.addNode(GenomeInterval(0,410,450));

    SVCandidateSetSequenceFragmentSampleGroup svDatagroup;

    // test a read is overlapping with a locus node when localnode's coordinate is GenomeInterval(0,80,120) and
    // remoteNode's coordinate is GenomeInterval(0,279,319). It will add an entry in svDatagroup.
    addSVNodeRead(bamHeader, scanner.operator*(), ((const SVLocus&)locus1).getNode(0), ((const SVLocus&)locus1).getNode(1),
            supplementSASplitRead, defaultReadGroupIndex, true, refSeq, true, false, svDatagroup, eCounts);
    BOOST_REQUIRE_EQUAL(svDatagroup.size(), 1u);

    // test a read is not overlapping with a locus node when localnode's coordinate is GenomeInterval(0,80,120) and
    // remoteNode's coordinate is GenomeInterval(0,279,319). It will not add any entry in svDatagroup.
    addSVNodeRead(bamHeader, scanner.operator*(), ((const SVLocus&)locus1).getNode(0), ((const SVLocus&)locus1).getNode(1),
            largeInsertionRead, defaultReadGroupIndex, true, refSeq, true, false, svDatagroup, eCounts);
    BOOST_REQUIRE_EQUAL(svDatagroup.size(), 1u);

    // test a read is overlapping with a locus node when localnode's coordinate is GenomeInterval(0,410,450) and
    // remoteNode's coordinate is GenomeInterval(0,279,319). It will add another entry in svDatagroup.
    addSVNodeRead(bamHeader, scanner.operator*(), ((const SVLocus&)locus1).getNode(2), ((const SVLocus&)locus1).getNode(1),
            largeInsertionRead, defaultReadGroupIndex, true, refSeq, true, false, svDatagroup, eCounts);
    BOOST_REQUIRE_EQUAL(svDatagroup.size(), 2u);
}

// test reference sequence of a segment. It will add 100 bases on both side
// that means if Genomic start and end coordinates are 1, and chromosome id
// is 0,  then the modified interval will be [max(0, 1-100), min(1+100, chrLength)).
// So the total length will be 101.
BOOST_AUTO_TEST_CASE( test_GetNodeRef)
{
    const bam_header_info bamHeader(buildTestBamHeader());
    SVLocus locus;
    locus.addNode(GenomeInterval(0,1,1));
    GenomeInterval searchInterval;
    reference_contig_segment refSeq;
    getNodeRefSeq(bamHeader, locus, 0, getTestReferenceFilename(), searchInterval, refSeq);
    // check the size first
    BOOST_REQUIRE_EQUAL(refSeq.seq().size(), 101);
    // check the sequence
    BOOST_REQUIRE_EQUAL(refSeq.seq(), "GATCACAGGTCTATCACCCTATTAACCACTCACGGGAGCTCTCCATGCATTTGGTATTTTCGTCTGGGGGGTGTGCACGCGATAGCATTGCGAGACGCTGG");
}

// test candidates must have at least evidence of 2
BOOST_AUTO_TEST_CASE( test_IsCandidateCountSufficient )
{
    SVCandidate candidate;
    for (unsigned i(0); i<SVEvidenceType::SIZE; ++i)
        candidate.bp1.lowresEvidence.add(i,1);

    // Evidence count is not sufficient
    BOOST_REQUIRE(!isCandidateCountSufficient(candidate));

    for (unsigned i(0); i<SVEvidenceType::SIZE; ++i)
        candidate.bp1.lowresEvidence.add(i,1);

    // Evidence count is sufficient
    BOOST_REQUIRE(isCandidateCountSufficient(candidate));


}

// test depth on each location i.e. number of
// read bases overlap in a location.
BOOST_AUTO_TEST_CASE( test_AddReadToDepthEst )
{
    bam_record bamRecord1;
    buildTestBamRecord(bamRecord1, 0, 200, 0, 210, 20, 15, "15M");
    bamRecord1.set_qname("Read-1");

    bam_record bamRecord2;
    buildTestBamRecord(bamRecord2, 0, 210, 0, 220, 20, 15, "15M");
    bamRecord2.set_qname("Read-2");

    std::vector<unsigned>depth(30);
    addReadToDepthEst(bamRecord1, 200, depth);
    addReadToDepthEst(bamRecord2, 200, depth);

    // test the coverage
    for (unsigned i = 0; i < 30; i++)
    {
        if (i >= 10 && i <= 19)
            BOOST_REQUIRE_EQUAL(depth[i], 2u); // second bamRead starts 10 bases after first bamRead
        else
            BOOST_REQUIRE_EQUAL(depth[i], 1u);
    }
}

// test the significance of a break point based on the supporting read
// observations relative to a background noise rate.
BOOST_AUTO_TEST_CASE( test_IsBreakPointSignificant )
{
    std::vector<double> signalReadInfo;

    // minimum signal count should be 2
    BOOST_REQUIRE(!isBreakPointSignificant(0.1, 0.5, signalReadInfo));

    // Break point is not significant as the probability that
    // the breakpoint is noise greater than the tolerance (0.005)
    signalReadInfo.push_back(96);
    signalReadInfo.push_back(158);
    signalReadInfo.push_back(163);
    BOOST_REQUIRE(!isBreakPointSignificant(0.005, 0.005, signalReadInfo));

    // Break point is significant as the probability that
    // the breakpoint is noise less than the tolerance (0.03)
    signalReadInfo.clear();
    signalReadInfo.push_back(3440);
    signalReadInfo.push_back(3443);
    signalReadInfo.push_back(3452);
    signalReadInfo.push_back(3489);
    BOOST_REQUIRE(isBreakPointSignificant(0.03, 0.008, signalReadInfo));
}

// test the significance of a spanning candidate for  minimum supporting evidence.
// Sppanning candidate is significant if either break point 1 or break point 2 is
// significant. This test verifies following cases:
// 1) When no breakpoint is significant.
// 2) When Breakpoint-1 is significant and Breakpoint-2 is not significant.
// 3) When Breakpoint-2 is significant and Breakpoint-1 is not significant.
// 4) When both the Breakpoints are significant.
BOOST_AUTO_TEST_CASE( test_IsSpanningCandidateSignalSignificant )
{
    SVCandidate svCandidate;
    FatSVCandidate fatSVCandidate(svCandidate, 1u);
    // Spanning candidate is not significant as none of the breakpoint
    // satisfies minimum evidence(2) criteria.
    BOOST_REQUIRE(!isSpanningCandidateSignalSignificant(0.008, fatSVCandidate, 0));

    // test when both breakpoint-1 and breakpoint-2 are not significant where
    // noise tolerance rate is 0.03.
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3468);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3520);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3569);

    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1403);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1428);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1480);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1507);
    BOOST_REQUIRE(!isSpanningCandidateSignalSignificant(0.008, fatSVCandidate, 0));

    // test when breakpoint-1 is significant as the probability that
    // the breakpoint-1 is noise less than the tolerance (0.03) and
    // breakpoint-2 is not significant.
    fatSVCandidate.bp1EvidenceIndex[0][0].clear();
    fatSVCandidate.bp2EvidenceIndex[0][0].clear();
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3489);

    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1403);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1428);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1480);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1507);
    BOOST_REQUIRE(isSpanningCandidateSignalSignificant(0.008, fatSVCandidate, 0));

    // test when breakpoint-2 is significant as the probability that
    // the breakpoint-2 is noise less than the tolerance (0.03) and
    // breakpoint-1 is not significant.
    fatSVCandidate.bp1EvidenceIndex[0][0].clear();
    fatSVCandidate.bp2EvidenceIndex[0][0].clear();
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(1403);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(1428);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(1480);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(1507);

    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(3489);
    BOOST_REQUIRE(isSpanningCandidateSignalSignificant(0.008, fatSVCandidate, 0));

    // test when both breakpoint-1 and breakpoint-2 are significant.
    fatSVCandidate.bp1EvidenceIndex[0][0].clear();
    fatSVCandidate.bp2EvidenceIndex[0][0].clear();
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3489);

    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1403);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1412);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1400);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1449);
    BOOST_REQUIRE(isSpanningCandidateSignalSignificant(0.008, fatSVCandidate, 0));
}

// test the significance of a complex candidate for  minimum supporting evidence
// where complex means that we have no specific hypothesis for the SV -
// it is just a single genomic region for which we schedule local assembly.
BOOST_AUTO_TEST_CASE( test_IsComplexCandidateSignalSignificant )
{
    SVCandidate svCandidate;
    FatSVCandidate fatSVCandidate(svCandidate, 1u);

    // Complex break point is not significant as the probability that
    // the breakpoint is noise greater than the tolerance (0.005) where
    // assembly rate is 0.008
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3489);
    BOOST_REQUIRE(!isComplexCandidateSignalSignificant(0.008, fatSVCandidate, 0));

    // Complex break point is significant as the probability that
    // the breakpoint is noise less than the tolerance (0.005) where
    // assembly rate is 0.008
    fatSVCandidate.bp1EvidenceIndex[0][0].clear();
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3448);
    BOOST_REQUIRE(isComplexCandidateSignalSignificant(0.008, fatSVCandidate, 0));
}

// test the significance of spanning candidate across all the bams
// relative to spanning noise rate. This test checks whether the method
// returns true if one of the bams has significant spanning candidate.
BOOST_AUTO_TEST_CASE( test_IsAnySpanningCandidateSignalSignificant )
{
    SVCandidate svCandidate;
    FatSVCandidate fatSVCandidate(svCandidate, 2u); // fat sv candidate object for 2 bams

    // insert read index values for 1st bam
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3489);

    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1403);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1428);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1480);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1507);

    // insert read index values for 2nd bam
    fatSVCandidate.bp1EvidenceIndex[0][1].push_back(3443);
    fatSVCandidate.bp1EvidenceIndex[0][1].push_back(3452);
    fatSVCandidate.bp1EvidenceIndex[0][1].push_back(3440);
    fatSVCandidate.bp1EvidenceIndex[0][1].push_back(3489);

    fatSVCandidate.bp2EvidenceIndex[0][1].push_back(1403);
    fatSVCandidate.bp2EvidenceIndex[0][1].push_back(1428);
    fatSVCandidate.bp2EvidenceIndex[0][1].push_back(1480);
    fatSVCandidate.bp2EvidenceIndex[0][1].push_back(1507);

    std::vector<double > spanningNoiseRate;
    spanningNoiseRate.push_back(0.008); // 1st bam spanning noise rate
    spanningNoiseRate.push_back(0.1); // 2nd bam spanning noise rate

    // spanning candidate is significant for 1st bam
    BOOST_REQUIRE(isAnySpanningCandidateSignalSignificant(1, fatSVCandidate, spanningNoiseRate));

    spanningNoiseRate.clear();
    spanningNoiseRate.push_back(0.1); // 1st bam spanning noise rate
    spanningNoiseRate.push_back(0.1); // 2nd bam spanning noise rate
    // spanning candidate is not significant for any of the bams
    BOOST_REQUIRE(!isAnySpanningCandidateSignalSignificant(1, fatSVCandidate, spanningNoiseRate));
}

// test the significance of complex candidate across all the bams
// relative to assembly noise rate. This test checks whether the method
// returns true if one of the bams has complex candidate.
BOOST_AUTO_TEST_CASE( test_IsAnyComplexCandidateSignalSignificant )
{
    SVCandidate svCandidate;
    FatSVCandidate fatSVCandidate(svCandidate, 2u); // fat sv candidate object for 2 bams

    // insert read index values for 1st bam
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate.bp1EvidenceIndex[0][0].push_back(3489);

    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1403);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1428);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1480);
    fatSVCandidate.bp2EvidenceIndex[0][0].push_back(1507);

    // insert values for 2nd bam
    fatSVCandidate.bp1EvidenceIndex[0][1].push_back(3443);
    fatSVCandidate.bp1EvidenceIndex[0][1].push_back(3452);
    fatSVCandidate.bp1EvidenceIndex[0][1].push_back(3440);
    fatSVCandidate.bp1EvidenceIndex[0][1].push_back(3489);

    fatSVCandidate.bp2EvidenceIndex[0][1].push_back(1403);
    fatSVCandidate.bp2EvidenceIndex[0][1].push_back(1428);
    fatSVCandidate.bp2EvidenceIndex[0][1].push_back(1480);
    fatSVCandidate.bp2EvidenceIndex[0][1].push_back(1507);

    std::vector<double > assemblyNoiseRate;
    assemblyNoiseRate.push_back(0.000002); // 1st bam assembly noise rate
    assemblyNoiseRate.push_back(0.008); // 2nd bam assembly noise rate

    // complex candidate is significant for 1st bam
    BOOST_REQUIRE(isAnyComplexCandidateSignalSignificant(1, fatSVCandidate, assemblyNoiseRate));

    assemblyNoiseRate.clear();
    assemblyNoiseRate.push_back(0.008); // 1st bam assembly noise rate
    assemblyNoiseRate.push_back(0.008); // 2nd bam assembly noise rate

    // complex candidate is not significant for any of the bams
    BOOST_REQUIRE(!isAnyComplexCandidateSignalSignificant(1, fatSVCandidate, assemblyNoiseRate));
}

// test the candidate's filtration state. This test verifies the following cases:
// 1. SEMI_MAPPED - When all evidence breakends are local
// 2. SPANNING_LOW_SIGNAL - Candidates support spanning SV, but none of them is significant
// spanning candidate(Significant spanning SV candidate has been described in test_IsAnySpanningCandidateSignalSignificant)
// 3. COMPLEX_LOW_COUNT - When a complex SV doesn't satisfy minimum candidate count criteria
// 4. COMPLEX_LOW_SIGNAL - Candidates support complex SV, but none of them is significant
// complex candidate(Significant complex SV candidate has been described in test_IsAnyComplexCandidateSignalSignificant)
// 5. None - None of the above filtration state
BOOST_AUTO_TEST_CASE( test_IsFilterSingleJunctionCandidate )
{
    SVCandidate svCandidate;
    FatSVCandidate fatSVCandidate1(svCandidate, 1u);

    fatSVCandidate1.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate1.bp1EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate1.bp1EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate1.bp1EvidenceIndex[0][0].push_back(3489);

    fatSVCandidate1.bp2EvidenceIndex[0][0].push_back(1403);
    fatSVCandidate1.bp2EvidenceIndex[0][0].push_back(1428);
    fatSVCandidate1.bp2EvidenceIndex[0][0].push_back(1480);
    fatSVCandidate1.bp2EvidenceIndex[0][0].push_back(1507);

    std::vector<double > assemblyNoiseRate;
    assemblyNoiseRate.push_back(0.000002);
    std::vector<double > spanningNoiseRate;
    spanningNoiseRate.push_back(0.008);

    // test for SEMI_MAPPED candidate as filtration state
    BOOST_REQUIRE_EQUAL(isFilterSingleJunctionCandidate(false, spanningNoiseRate, assemblyNoiseRate, fatSVCandidate1, 1), SINGLE_JUNCTION_FILTER::SEMI_MAPPED);

    svCandidate.bp1.state = SVBreakendState::index_t::RIGHT_OPEN ;
    svCandidate.bp2.state = SVBreakendState::index_t::LEFT_OPEN ;
    svCandidate.bp1.lowresEvidence.add(0, 2);
    FatSVCandidate fatSVCandidate2(svCandidate,1);
    fatSVCandidate2.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate2.bp1EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate2.bp1EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate2.bp1EvidenceIndex[0][0].push_back(3489);

    fatSVCandidate2.bp2EvidenceIndex[0][0].push_back(1403);
    fatSVCandidate2.bp2EvidenceIndex[0][0].push_back(1428);
    fatSVCandidate2.bp2EvidenceIndex[0][0].push_back(1480);
    fatSVCandidate2.bp2EvidenceIndex[0][0].push_back(1507);

    // When none of the filtration state is satisfied
    spanningNoiseRate.clear();
    spanningNoiseRate.push_back(0.008);
    BOOST_REQUIRE_EQUAL(isFilterSingleJunctionCandidate(false, spanningNoiseRate, assemblyNoiseRate, fatSVCandidate2, 1),
            SINGLE_JUNCTION_FILTER::NONE);

    spanningNoiseRate.clear();
    spanningNoiseRate.push_back(0.1);
    // test for SPANNING_LOW_SIGNAL as filtration state
    BOOST_REQUIRE_EQUAL(isFilterSingleJunctionCandidate(false, spanningNoiseRate, assemblyNoiseRate, fatSVCandidate2, 1),
            SINGLE_JUNCTION_FILTER::SPANNING_LOW_SIGNAL);

    // test for COMPLEX_LOW_COUNT as filtration state
    svCandidate.bp1.state = SVBreakendState::index_t::COMPLEX ;
    svCandidate.bp2.state = SVBreakendState::index_t::UNKNOWN ;
    svCandidate.bp1.lowresEvidence.add(0, 2);
    FatSVCandidate fatSVCandidate3(svCandidate,1);
    BOOST_REQUIRE_EQUAL(isFilterSingleJunctionCandidate(false, spanningNoiseRate, assemblyNoiseRate, fatSVCandidate3, 1),
            SINGLE_JUNCTION_FILTER::COMPLEX_LOW_COUNT);

    // test for COMPLEX_LOW_SIGNAL as filtration state
    svCandidate.bp1.state = SVBreakendState::index_t::COMPLEX ;
    svCandidate.bp2.state = SVBreakendState::index_t::UNKNOWN ;
    svCandidate.bp1.lowresEvidence.clear();
    svCandidate.bp1.lowresEvidence.add(2, 3);
    FatSVCandidate fatSVCandidate4(svCandidate,1);
    BOOST_REQUIRE_EQUAL(isFilterSingleJunctionCandidate(false, spanningNoiseRate, assemblyNoiseRate, fatSVCandidate4, 1),
            SINGLE_JUNCTION_FILTER::COMPLEX_LOW_SIGNAL);
}


// test filters on all SV candidates. Following candidates will be filtered out
// 1. Semi Mapped
// 2. COMPLEX LOW COUNT
// 3. COMPLEX LOW SIGNAL
// This test also  checks the delaying process for filtering Spanning_Low_Signal candidates.
BOOST_AUTO_TEST_CASE( test_filterCandidates )
{
    SVCandidate svCandidate1;
    FatSVCandidate fatSVCandidate1(svCandidate1, 1u);

    fatSVCandidate1.bp1EvidenceIndex[0][0].push_back(3443);
    fatSVCandidate1.bp1EvidenceIndex[0][0].push_back(3452);
    fatSVCandidate1.bp1EvidenceIndex[0][0].push_back(3440);
    fatSVCandidate1.bp1EvidenceIndex[0][0].push_back(3489);

    fatSVCandidate1.bp2EvidenceIndex[0][0].push_back(1403);
    fatSVCandidate1.bp2EvidenceIndex[0][0].push_back(1428);
    fatSVCandidate1.bp2EvidenceIndex[0][0].push_back(1480);
    fatSVCandidate1.bp2EvidenceIndex[0][0].push_back(1507);

    std::vector<double > assemblyNoiseRate;
    assemblyNoiseRate.push_back(0.000002);
    std::vector<double > spanningNoiseRate;
    spanningNoiseRate.push_back(0.008);

    std::vector<FatSVCandidate> svCandidates;
    // SEMI_MAPPED SV candidate. It should be filtered out.
    svCandidates.push_back(fatSVCandidate1);

    // spanning Low signal SV Cnadidate. It should not be filtered out.
    SVCandidate svCandidate2;
    svCandidate2.bp1.state = SVBreakendState::index_t::RIGHT_OPEN ;
    svCandidate2.bp2.state = SVBreakendState::index_t::LEFT_OPEN ;
    svCandidate2.bp1.lowresEvidence.add(0, 2);
    FatSVCandidate fatSVCandidate2(svCandidate2,1);
    svCandidates.push_back(fatSVCandidate2);

    // COMPLEX LOW COUNT SV Candidate. It should be filtered out.
    SVCandidate svCandidate3;
    svCandidate3.bp1.state = SVBreakendState::index_t::COMPLEX ;
    svCandidate3.bp2.state = SVBreakendState::index_t::UNKNOWN ;
    svCandidate3.bp1.lowresEvidence.add(0, 2);
    FatSVCandidate fatSVCandidate3(svCandidate3,1);
    svCandidates.push_back(fatSVCandidate3);

    // COMPLEX LOW SIGNAL. It should be filtered out.
    SVCandidate svCandidate4;
    svCandidate4.bp1.state = SVBreakendState::index_t::COMPLEX ;
    svCandidate4.bp2.state = SVBreakendState::index_t::UNKNOWN ;
    svCandidate4.bp1.lowresEvidence.clear();
    svCandidate4.bp1.lowresEvidence.add(2, 3);
    FatSVCandidate fatSVCandidate4(svCandidate4,1);
    svCandidates.push_back(fatSVCandidate4);
    SVFinderStats stats;
    filterCandidates(false, spanningNoiseRate, assemblyNoiseRate, svCandidates, stats, 1);

    // check all the stats
    BOOST_REQUIRE_EQUAL(svCandidates.size(), 1);
    BOOST_REQUIRE_EQUAL(stats.ComplexLowCountFilter, 1);
    BOOST_REQUIRE_EQUAL(stats.ComplexLowSignalFilter, 1);
    BOOST_REQUIRE_EQUAL(stats.semiMappedFilter, 1);

    // check whether spanning Low signal sv candidate is there or not. It should not be filtered out.
    BOOST_REQUIRE_EQUAL(svCandidates[0].bp1.state, SVBreakendState::index_t::RIGHT_OPEN);
    BOOST_REQUIRE_EQUAL(svCandidates[0].bp2.state, SVBreakendState::index_t::LEFT_OPEN);
    // test whether spanning Low signal sv candidate is marked for a multi-junction evaluation.
    BOOST_REQUIRE(svCandidates[0].isSingleJunctionFilter);
}

// updateEvidenceIndex stores additional bam read index to decide if the candidate evidence
// is significant relative to background noise in the sample. The significance of SV candidate
// has been described in test_IsSpanningCandidateSignalSignificant and
// test_IsComplexCandidateSignalSignificant test cases. This unit test checks whether
// updateEvidenceIndex method stores read index correctly for different SV evidence. This test
// checks the read index based on nature of SV evidence provided by a single DNA/RNA fragment.
BOOST_AUTO_TEST_CASE( test_updateEvidenceIndex )
{
    bam_record bamRecord1;
    buildTestBamRecord(bamRecord1, 0, 200, 0, 210, 20, 15, "15M");
    bamRecord1.set_qname("Read-1");

    SVCandidateSetSequenceFragment fragment;
    SVObservation svObservation;
    // single source sv evidence
    svObservation.dnaFragmentSVEvidenceSource = SourceOfSVEvidenceInDNAFragment::READ1;
    fragment.read1.bamrec = bamRecord1;
    fragment.read1.readIndex = 1; // setting the read index

    // check read index for Semi align evidence type
    svObservation.svEvidenceType = SVEvidenceType::SEMIALIGN;
    SVCandidate svCandidate;
    FatSVCandidate fatSVCandidate(svCandidate, 1u);
    updateEvidenceIndex(fragment, svObservation, fatSVCandidate, 0);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp1EvidenceIndex[SVEvidenceType::SEMIALIGN][0][0], 1);

    // check read index for SPLIT_ALIGN align evidence type
    svObservation.svEvidenceType = SVEvidenceType::SPLIT_ALIGN;
    updateEvidenceIndex(fragment, svObservation, fatSVCandidate, 0);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp1EvidenceIndex[SVEvidenceType::SPLIT_ALIGN][0].size(), 1);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp2EvidenceIndex[SVEvidenceType::SPLIT_ALIGN][0].size(), 0);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp1EvidenceIndex[SVEvidenceType::SPLIT_ALIGN][0][0], 1);

    // Adding supplementary read
    bam_record supplementSASplitRead;
    buildTestBamRecord(supplementSASplitRead);
    addSupplementaryAlignmentEvidence(supplementSASplitRead);

    SVCandidateSetRead svCandidateSetRead;
    svCandidateSetRead.bamrec = supplementSASplitRead;
    fragment.read1Supplemental.push_back(svCandidateSetRead);
    updateEvidenceIndex(fragment, svObservation, fatSVCandidate, 0);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp1EvidenceIndex[SVEvidenceType::SPLIT_ALIGN][0].size(), 2);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp2EvidenceIndex[SVEvidenceType::SPLIT_ALIGN][0].size(), 1);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp1EvidenceIndex[SVEvidenceType::SPLIT_ALIGN][0][1], 1);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp2EvidenceIndex[SVEvidenceType::SPLIT_ALIGN][0][0], 0);

    // check read index for PAIR evidence type
    bam_record bamRecord2;
    buildTestBamRecord(bamRecord2, 0, 210, 0, 220, 20, 15, "15M");
    bamRecord2.set_qname("Read-2");

    // SV evidence source is Pair Reads
    svObservation.dnaFragmentSVEvidenceSource = SourceOfSVEvidenceInDNAFragment::READ_PAIR;
    fragment.read1Supplemental.clear();
    fragment.read2.bamrec = bamRecord2;
    fragment.read2.readIndex = 2;
    svObservation.svEvidenceType = SVEvidenceType::PAIR;
    updateEvidenceIndex(fragment, svObservation, fatSVCandidate, 0);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp1EvidenceIndex[SVEvidenceType::PAIR][0].size(), 1);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp2EvidenceIndex[SVEvidenceType::PAIR][0].size(), 1);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp1EvidenceIndex[SVEvidenceType::PAIR][0][0], 1);
    BOOST_REQUIRE_EQUAL(fatSVCandidate.bp2EvidenceIndex[SVEvidenceType::PAIR][0][0], 2);
}

BOOST_AUTO_TEST_SUITE_END()