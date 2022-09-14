#ifndef SAM_HPP
#define SAM_HPP

#include <string>
#include "kseq++.hpp"
#include "refs.hpp"

using namespace klibpp;


struct alignment {
    std::string cigar;
    int ref_start;
    int ed;
    int global_ed;
    int sw_score;
    int aln_score;
    int ref_id;
    int mapq;
    int aln_length;
    bool not_proper;
    bool is_rc;
    bool is_unaligned = false;
};

enum SamFlags {
    PAIRED = 1,
    PROPER_PAIR = 2,
    UNMAP = 4,
    MUNMAP = 8,
    REVERSE = 0x10,
    MREVERSE = 0x20,
    READ1 = 0x40,
    READ2 = 0x80,
    SECONDARY = 0x100,
    QCFAIL = 0x200,
    DUP = 0x400,
    SUPPLEMENTARY = 0x800,
};

class Sam {

public:
    Sam(std::string& sam_string, const ref_names& reference_names)
        : sam_string(sam_string)
        , reference_names(reference_names) { }

    /* Add an alignment */
    void add(const alignment& sam_aln, const KSeq& record, const std::string& sequence_rc, bool is_secondary = false);
    void add_pair(alignment &sam_aln1, alignment &sam_aln2, const KSeq& record1, const KSeq& record2, const std::string &read1_rc, const std::string &read2_rc, int &mapq1, int &mapq2, float &mu, float &sigma, bool is_primary);
    void add_unmapped(const KSeq& record, int flags = UNMAP);
    void add_unmapped_pair(const KSeq& r1, const KSeq& r2);

    void add_one(const KSeq& record, int flags, const std::string& ref_name, const alignment& sam_aln, int mapq, const std::string& mate_name, int mate_ref_start, int template_len, const std::string& output_read, int ed);

private:
    std::string& sam_string;
    const ref_names& reference_names;
};


#endif