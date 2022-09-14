//
//  index.cpp
//  cpp_course
//
//  Created by Kristoffer Sahlin on 4/21/21.
//
#include "index.hpp"

#include <iostream>
#include <math.h>       /* pow */
#include <bitset>
#include <climits>
#include <inttypes.h>
#include <fstream>
#include <cassert>

#include "logger.hpp"

static Logger& logger = Logger::get();


// a, A -> 0
// c, C -> 1
// g, G -> 2
// t, T, u, U -> 3
static unsigned char seq_nt4_table[256] = {
        0, 1, 2, 3,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 0, 4, 1,  4, 4, 4, 2,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  3, 3, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 0, 4, 1,  4, 4, 4, 2,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  3, 3, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
        4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4
};

uint64_t count_unique_elements(const hash_vector& h_vector){
    assert(h_vector.size() > 0);
    uint64_t prev_k = h_vector[0];
    uint64_t unique_elements = 1;
    for (auto &curr_k : h_vector) {
        if (curr_k != prev_k) {
            unique_elements ++;
        }
        prev_k = curr_k;
    }
    return unique_elements;
}

unsigned int index_vector(const hash_vector &h_vector, kmer_lookup &mers_index, float f) {
    logger.debug() << "Flat vector size: " << h_vector.size() << std::endl;
    unsigned int offset = 0;
    unsigned int prev_offset = 0;
    unsigned int count = 0;

    unsigned int tot_occur_once = 0;
    unsigned int tot_high_ab = 0;
    unsigned int tot_mid_ab = 0;
    std::vector<unsigned int> strobemer_counts;

    uint64_t prev_k = h_vector[0];
    uint64_t curr_k;

    for ( auto &t : h_vector) {
        curr_k = t;
        if (curr_k == prev_k){
            count ++;
        }
        else {
            if (count == 1){
                tot_occur_once ++;
            }
            else if (count > 100){
                tot_high_ab ++;
                strobemer_counts.push_back(count);
            }
            else{
                tot_mid_ab ++;
                strobemer_counts.push_back(count);
            }

            std::tuple<unsigned int, unsigned int> s(prev_offset, count);
            mers_index[prev_k] = s;
            count = 1;
            prev_k = curr_k;
            prev_offset = offset;
        }
        offset ++;
    }

    // last k-mer
    std::tuple<unsigned int, unsigned int> s(prev_offset, count);
    mers_index[curr_k] = s;
    float frac_unique = ((float) tot_occur_once )/ mers_index.size();
    logger.debug()
        << "Total strobemers count: " << offset << std::endl
        << "Total strobemers occur once: " << tot_occur_once << std::endl
        << "Fraction Unique: " << frac_unique << std::endl
        << "Total strobemers highly abundant > 100: " << tot_high_ab << std::endl
        << "Total strobemers mid abundance (between 2-100): " << tot_mid_ab << std::endl
        << "Total distinct strobemers stored: " << mers_index.size() << std::endl;
    if (tot_high_ab >= 1) {
        logger.debug() << "Ratio distinct to highly abundant: " << mers_index.size() / tot_high_ab << std::endl;
    }
    if (tot_mid_ab >= 1) {
        logger.debug() << "Ratio distinct to non distinct: " << mers_index.size() / (tot_high_ab + tot_mid_ab) << std::endl;
    }
    // get count for top -f fraction of strobemer count to filter them out
    std::sort(strobemer_counts.begin(), strobemer_counts.end(), std::greater<int>());

    unsigned int index_cutoff = mers_index.size()*f;
    logger.debug() << "Filtered cutoff index: " << index_cutoff << std::endl;
    unsigned int filter_cutoff;
    if (!strobemer_counts.empty()){
        filter_cutoff =  index_cutoff < strobemer_counts.size() ?  strobemer_counts[index_cutoff] : strobemer_counts.back();
        filter_cutoff = filter_cutoff > 30 ? filter_cutoff : 30; // cutoff is around 30-50 on hg38. No reason to have a lower cutoff than this if aligning to a smaller genome or contigs.
        filter_cutoff = filter_cutoff > 100 ? 100 : filter_cutoff; // limit upper cutoff for normal NAM finding - use rescue mode instead
    } else {
        filter_cutoff = 30;
    }
    logger.debug() << "Filtered cutoff count: " << filter_cutoff << std::endl << std::endl;
    return filter_cutoff;
}


void write_index(const st_index& index, const References& references, const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary);

    //write filter_cutoff
    ofs.write(reinterpret_cast<const char*>(&index.filter_cutoff), sizeof(index.filter_cutoff));

    //write ref_seqs:
    uint64_t s1 = uint64_t(references.sequences.size());
    ofs.write(reinterpret_cast<char*>(&s1), sizeof(s1));
    //For each string, write length and then the string
    uint32_t s2 = 0;
    for (std::size_t i = 0; i < references.sequences.size(); ++i) {
        s2 = uint32_t(references.sequences[i].length());
        ofs.write(reinterpret_cast<char*>(&s2), sizeof(s2));
        ofs.write(references.sequences[i].c_str(), references.sequences[i].length());
    }
    
    //write ref_lengths:
    //write everything in one large chunk
    s1 = uint64_t(references.lengths.size());
    ofs.write(reinterpret_cast<char*>(&s1), sizeof(s1));
    ofs.write(reinterpret_cast<const char*>(&references.lengths[0]), references.lengths.size()*sizeof(references.lengths[0]));

    //write acc_map:
    s1 = uint64_t(references.names.size());
    ofs.write(reinterpret_cast<char*>(&s1), sizeof(s1));
    //For each string, write length and then the string
    for (std::size_t i = 0; i < references.names.size(); ++i) {
        s2 = uint32_t(references.names[i].length());
        ofs.write(reinterpret_cast<char*>(&s2), sizeof(s2));
        ofs.write(references.names[i].c_str(), references.names[i].length());
    }

    //write flat_vector:
    s1 = uint64_t(index.flat_vector.size());
    ofs.write(reinterpret_cast<char*>(&s1), sizeof(s1));
    ofs.write(reinterpret_cast<const char*>(&index.flat_vector[0]), index.flat_vector.size() * sizeof(index.flat_vector[0]));

    //write mers_index:
    s1 = uint64_t(index.mers_index.size());
    ofs.write(reinterpret_cast<char*>(&s1), sizeof(s1));
    for (auto& p : index.mers_index) {
        ofs.write(reinterpret_cast<const char*>(&p.first), sizeof(p.first));
        ofs.write(reinterpret_cast<const char*>(&p.second), sizeof(p.second));
    }
};

void read_index(st_index& index, References& references, const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    //read filter_cutoff
    ifs.read(reinterpret_cast<char*>(&index.filter_cutoff), sizeof(index.filter_cutoff));

    //read ref_seqs:
    references.sequences.clear();
    uint64_t sz = 0;
    ifs.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    references.sequences.reserve(sz);
    uint32_t sz2 = 0;
    auto& refseqs = references.sequences;
    for (uint64_t i = 0; i < sz; ++i) {
        ifs.read(reinterpret_cast<char*>(&sz2), sizeof(sz2));
        std::unique_ptr<char> buf_ptr(new char[sz2]);//The vector is short with large strings, so allocating this way should be ok.
        ifs.read(buf_ptr.get(), sz2);
        //we could potentially use std::move here to avoid reallocation, something like std::string(std::move(buf), sz2), but it has to be investigated more
        refseqs.push_back(std::string(buf_ptr.get(), sz2));
    }

    //read ref_lengths
    ////////////////
    references.lengths.clear();
    ifs.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    references.lengths.resize(sz); //annoyingly, this initializes the memory to zero (which is a waste of performance), but ignore that for now
    ifs.read(reinterpret_cast<char*>(&references.lengths[0]), sz*sizeof(references.lengths[0]));

    //read acc_map:
    references.names.clear();
    ifs.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    references.names.reserve(sz);
    auto& acc_map = references.names;

    for (int i = 0; i < sz; ++i) {
        ifs.read(reinterpret_cast<char*>(&sz2), sizeof(sz2));
        std::unique_ptr<char> buf_ptr(new char[sz2]);
        ifs.read(buf_ptr.get(), sz2);
        acc_map.push_back(std::string(buf_ptr.get(), sz2));
    }

    //read flat_vector:
    index.flat_vector.clear();
    ifs.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    index.flat_vector.resize(sz); //annoyingly, this initializes the memory to zero (which is a waste of performance), but let's ignore that for now
    ifs.read(reinterpret_cast<char*>(&index.flat_vector[0]), sz*sizeof(index.flat_vector[0]));

    //read mers_index:
    index.mers_index.clear();
    ifs.read(reinterpret_cast<char*>(&sz), sizeof(sz));
    index.mers_index.reserve(sz);
    //read in big chunks
    const uint64_t chunk_size = pow(2,20);//4 M => chunks of ~10 MB - The chunk size seem not to be that important
    auto buf_size = std::min(sz, chunk_size) * (sizeof(kmer_lookup::key_type) + sizeof(kmer_lookup::mapped_type));
    std::unique_ptr<char> buf_ptr(new char[buf_size]);
    char* buf2 = buf_ptr.get();
    auto left_to_read = sz;
    while (left_to_read > 0) {
        auto to_read = std::min(left_to_read, chunk_size);
        ifs.read(buf2, to_read * (sizeof(kmer_lookup::key_type) + sizeof(kmer_lookup::mapped_type)));
        //Add the elements directly from the buffer
        for (int i = 0; i < to_read; ++i) {
            auto start = buf2 + i * (sizeof(kmer_lookup::key_type) + sizeof(kmer_lookup::mapped_type));
            index.mers_index[*reinterpret_cast<kmer_lookup::key_type*>(start)] = *reinterpret_cast<kmer_lookup::mapped_type*>(start + sizeof(kmer_lookup::key_type));
        }
        left_to_read -= to_read;
    }
};


// update queue and current minimum and position
static inline void update_window(std::deque <uint64_t> &q, std::deque <unsigned int> &q_pos, uint64_t &q_min_val, int &q_min_pos, uint64_t new_strobe_hashval, int i, bool &new_minimizer){
//    uint64_t popped_val;
//    popped_val = q.front();
    q.pop_front();

    unsigned int popped_index;
    popped_index=q_pos.front();
    q_pos.pop_front();

    q.push_back(new_strobe_hashval);
    q_pos.push_back(i);
    if (q_min_pos == popped_index){ // we popped the previous minimizer, find new brute force
//    if (popped_val == q_min_val){ // we popped the minimum value, find new brute force
        q_min_val = UINT64_MAX;
        q_min_pos = i;
        for (int j = q.size() - 1; j >= 0; j--) { //Iterate in reverse to choose the rightmost minimizer in a window
//        for (int j = 0; j <= q.size()-1; j++) {
            if (q[j] < q_min_val) {
                q_min_val = q[j];
                q_min_pos = q_pos[j];
                new_minimizer = true;
            }
        }
    }
    else if ( new_strobe_hashval < q_min_val ) { // the new value added to queue is the new minimum
        q_min_val = new_strobe_hashval;
        q_min_pos = i;
        new_minimizer = true;
    }
}

static inline void make_string_to_hashvalues_open_syncmers_canonical(const std::string &seq, std::vector<uint64_t> &string_hashes, std::vector<unsigned int> &pos_to_seq_choord, uint64_t kmask, int k, uint64_t smask, int s, int t)
{
    std::deque<uint64_t> qs;
    std::deque<unsigned int> qs_pos;
    int seq_length = seq.length();
    int qs_size = 0;
    uint64_t qs_min_val = UINT64_MAX;
    int qs_min_pos = -1;


//    robin_hood::hash<uint64_t> robin_hash;
    uint64_t mask = (1ULL<<2*k) - 1;
//    std::cerr << mask << std::endl;

//    std::vector<std::tuple<uint64_t, unsigned int, unsigned int> > kmers;
    int gap = 0;
    std::string subseq;
    unsigned int hash_count = 0;
    int l;
    uint64_t xk[2];
    xk[0] = xk[1] = 0;
    uint64_t xs[2];
    xs[0] = xs[1] = 0;
    uint64_t kshift = (k - 1) * 2;
    uint64_t sshift = (s - 1) * 2;
    for (int i = l = 0; i < seq_length; i++) {
        int c = seq_nt4_table[(uint8_t) seq[i]];
        if (c < 4) { // not an "N" base
            xk[0] = (xk[0] << 2 | c) & kmask;                  // forward strand
            xk[1] = xk[1] >> 2 | (uint64_t)(3 - c) << kshift;  // reverse strand
            xs[0] = (xs[0] << 2 | c) & smask;                  // forward strand
            xs[1] = xs[1] >> 2 | (uint64_t)(3 - c) << sshift;  // reverse strand
            if (++l >= s) { // we find an s-mer
                uint64_t ys = std::min(xs[0], xs[1]);
//                uint64_t hash_s = robin_hash(ys);
                uint64_t hash_s = ys;
//                uint64_t hash_s = hash64(ys, mask);
//                uint64_t hash_s = XXH64(&ys, 8,0);
                // queue not initialized yet
                if (qs_size < k - s ) {
                    qs.push_back(hash_s);
                    qs_pos.push_back(i - s + 1);
                    qs_size++;
                }
                else if (qs_size == k - s ) { // We are here adding the last s-mer and have filled queue up, need to decide for this k-mer (the first encountered) if we are adding it/
                    qs.push_back(hash_s);
                    qs_pos.push_back(i - s + 1);
                    qs_size++;
//                    std::cerr << qs_size << " "<< i - k + 1 << std::endl;
                    for (int j = 0; j < qs_size; j++) {
//                        std::cerr << qs_pos[j] << " " << qs[j] << " " << qs_min_val << std::endl;
                        if (qs[j] < qs_min_val) {
                            qs_min_val = qs[j];
                            qs_min_pos = qs_pos[j];
                        }
                    }
                    if (qs_min_pos == qs_pos[t-1]) { // occurs at t:th position in k-mer
//                    if ( (qs_min_pos == qs_pos[t-1]) || ((gap > 10) && ((qs_min_pos == qs_pos[k - s]) || (qs_min_pos == qs_pos[0]))) ) { // occurs at first or last position in k-mer
                        uint64_t yk = std::min(xk[0], xk[1]);
//                        uint64_t hash_k = robin_hash(yk);
//                        uint64_t hash_k = yk;
//                        uint64_t hash_k =  hash64(yk, mask);
                        uint64_t hash_k = XXH64(&yk, 8,0);
//                        uint64_t hash_k =  sahlin_dna_hash(yk, mask);
                        string_hashes.push_back(hash_k);
                        pos_to_seq_choord.push_back(i - k + 1);
                        hash_count++;
//                        std::cerr << i - s + 1 << " " << i - k + 1 << " " << (xk[0] < xk[1]) << std::endl;
//                        std::cerr <<  "Sampled gap: " << gap (k-s+1) << std::endl;
                        gap = 0;
                    }
                }
                else{
                    bool new_minimizer = false;
                    update_window(qs, qs_pos, qs_min_val, qs_min_pos, hash_s, i - s + 1, new_minimizer );
                    if (qs_min_pos == qs_pos[t-1]) { // occurs at t:th position in k-mer
//                    if ( (qs_min_pos == qs_pos[t-1]) || ((gap > 10) && ((qs_min_pos == qs_pos[k - s]) || (qs_min_pos == qs_pos[0]))) ) { // occurs at first or last position in k-mer
//                        if ( (gap > k) && (gap < 200) ) { // open syncmers no window guarantee, fill in subsequence with closed syncmers
//                            subseq = seq.substr(i - k + 1 - gap + 1, gap +k);
//                            make_string_to_hashvalues_closed_syncmers_canonical(subseq, string_hashes, pos_to_seq_choord, kmask, k, smask, s, t, i - k + 1 - gap + 1);
//                        }

                        uint64_t yk = std::min(xk[0], xk[1]);
//                        uint64_t hash_k = robin_hash(yk);
//                        uint64_t hash_k = yk;
//                        uint64_t hash_k = hash64(yk, mask);
                        uint64_t hash_k = XXH64(&yk, 8, 0);
//                        uint64_t hash_k =  sahlin_dna_hash(yk, mask);
                        string_hashes.push_back(hash_k);
                        pos_to_seq_choord.push_back(i - k + 1);
//                        std::cerr << i - k + 1 << std::endl;
                        hash_count++;
//                        std::cerr << i - s + 1 << " " << i - k + 1 << " " << (xk[0] < xk[1]) << std::endl;
//                        std::cerr <<  "Gap: " << gap << " position:" << i - k + 1 << std::endl;
                        gap = 0;
                    }
                    gap ++;
                }
//                if (gap > 25){
//                    std::cerr <<  "Gap: " << gap << " position:" << i - k + 1 << std::endl;
//                    if (gap < 500 ) {
//                        std::cerr << seq.substr(i - k + 1 - gap + 1, gap +k) << std::endl;
//                    }
//                }
            }
        } else {
            // if there is an "N", restart
            qs_min_val = UINT64_MAX;
            qs_min_pos = -1;
            l = xs[0] = xs[1] = xk[0] = xk[1] = 0;
            qs_size = 0;
            qs.clear();
            qs_pos.clear();
        }
    }
//    std::cerr << hash_count << " values produced from string of length " <<   seq_length << std::endl;
//    for(auto t: pos_to_seq_choord){
//        std::cerr << t << " ";
//    }
//    std::cerr << " " << std::endl;
}

//
//static inline void make_string_to_hashvalues_random_minimizers(std::string &seq, std::vector<uint64_t> &string_hashes, std::vector<unsigned int> &pos_to_seq_choord, int k, uint64_t kmask, int w) {
//    // initialize the deque
//    std::deque <uint64_t> q;
//    std::deque <unsigned int> q_pos;
//    int seq_length = seq.length();
//    int q_size = 0;
//    uint64_t q_min_val = UINT64_MAX;
//    int q_min_pos = -1;
//
//
//    robin_hood::hash<uint64_t> robin_hash;
////    std::vector<std::tuple<uint64_t, unsigned int, unsigned int> > kmers;
//    unsigned int hash_count = 0;
//    int l;
//    int i;
//    uint64_t x = 0;
//    for (int i = l = 0; i < seq_length; i++) {
//        int c = seq_nt4_table[(uint8_t) seq[i]];
//        if (c < 4) { // not an "N" base
//            x = (x << 2 | c) & kmask;                  // forward strand
//            if (++l >= k) { // we find a k-mer
//                uint64_t hash_k = robin_hash(x);
//
//                // que not initialized yet
//                if (q_size < w){
//                    q.push_back(hash_k);
//                    q_pos.push_back(i-k+1);
//                    q_size ++;
//                }
//                // que just filled up
//                else if (q_size == w - 1){
//                    q.push_back(hash_k);
//                    q_pos.push_back(i-k+1);
//                    q_min_val = UINT64_MAX;
//                    q_min_pos = -1;
//                    for (int j = 0; j <= w-1; j++) {
//                        if (q[j] < q_min_val) {
//                            q_min_val = q[j];
//                            q_min_pos = q_pos[j];
//                        }
//                    }
//                    string_hashes.push_back(q_min_val);
//                    pos_to_seq_choord.push_back(q_min_pos);
//                    hash_count ++;
//                }
//                // sliding the queue
//                else{
//                    bool new_minimizer = false;
//                    update_window(q, q_pos, q_min_val, q_min_pos, hash_k, i - k + 1, new_minimizer );
//                    if (new_minimizer) {
//                        string_hashes.push_back(q_min_val);
//                        pos_to_seq_choord.push_back(q_min_pos);
//                        hash_count++;
//                    }
//                }
//
//            }
//        } else {
//            l = 0, x = 0; // if there is an "N", restart
//        }
//    }
////    std::cerr << hash_count << " values produced from string of length " <<   seq_length << std::endl;
////    for(auto t: pos_to_seq_choord){
////        std::cerr << t << " ";
////    }
////    std::cerr << " " << std::endl;
//}



static inline void get_next_strobe(const std::vector<uint64_t> &string_hashes, uint64_t strobe_hashval, unsigned int &strobe_pos_next, uint64_t &strobe_hashval_next, unsigned int w_start, unsigned int w_end, uint64_t q){
    uint64_t min_val = UINT64_MAX;
//    int max_val = INT_MIN;
//    int min_val = INT_MAX;
//    int res;
//    std::bitset<64> b1,b2;
//        uint64_t rot_strobe_hashval = (strobe_hashval << q)|(strobe_hashval >> (64 - q));
//    uint64_t shift_strobe_hashval = strobe_hashval >> 5;
    std::bitset<64> b;
//    int a,b;
//    int p = pow (2, 4) - 1;
//    a = strobe_hashval & p;
//    int c = b1.count();
//    int d;

//    unsigned int min_pos;
//    min_pos = -1;
    for (auto i = w_start; i <= w_end; i++) {
//         Method 2
//        uint64_t res = (strobe_hashval + string_hashes[i]) & q;

//         Method 1
//        uint64_t res = (strobe_hashval + string_hashes[i]) % q;

        // Method 3 - seems to give the best tradeoff in speed and accuracy at this point
//        b = (strobe_hashval ^ string_hashes[i]);
//        uint64_t res = b.count();

        // Method 3' skew sample more for prob exact matching
        b = (strobe_hashval ^ string_hashes[i])  & q;
        uint64_t res = b.count();

        // Method by Lidon Gao (other strobemers library) and Giulio Ermanno Pibiri @giulio_pibiri
//        uint64_t res = (strobe_hashval ^ string_hashes[i]) ;

        // Method 6 Sahlin introduce skew (Method 3 and 3' are symmetrical for comp value of (s1,s2) and (s2,s1)
        // Methods 6 introduce asymmetry to reduce prob that we pick (s1,s2) and (s2,s1) as strobes to minimize fw and rc collisions
//        b = (shift_strobe_hashval ^ string_hashes[i])  & q;
//        uint64_t res = b.count();

        // Method 7 minimize collisions while still keeping small values space:
//        b = string_hashes[i] & p;
//        int res = a - b;

        if (res < min_val){
            min_val = res;
            strobe_pos_next = i;
//            std::cerr << strobe_pos_next << " " << min_val << std::endl;
            strobe_hashval_next = string_hashes[i];
        }
    }
//    std::cerr << "Offset: " <<  strobe_pos_next - w_start << " val: " << min_val <<  ", P exact:" <<  1.0 - pow ( (float) (8-min_val)/9, strobe_pos_next - w_start) << std::endl;

}


static inline void get_next_strobe_dist_constraint(const std::vector<uint64_t> &string_hashes, const std::vector<unsigned int> &pos_to_seq_choord, uint64_t strobe_hashval, unsigned int &strobe_pos_next, uint64_t &strobe_hashval_next,  unsigned int w_start, unsigned int w_end, uint64_t q, unsigned int seq_start, unsigned int seq_end_constraint, unsigned int strobe1_start)
{
    uint64_t min_val = UINT64_MAX;
    strobe_pos_next = strobe1_start; // Defaults if no nearby syncmer
    strobe_hashval_next = string_hashes[strobe1_start];
    std::bitset<64> b;

    for (auto i = w_start; i <= w_end; i++) {

        // Method 3' skew sample more for prob exact matching
        b = (strobe_hashval ^ string_hashes[i])  & q;
        uint64_t res = b.count();

        if (pos_to_seq_choord[i] > seq_end_constraint){
            return;
        }

        if (res < min_val){
            min_val = res;
            strobe_pos_next = i;
//            std::cerr << strobe_pos_next << " " << min_val << std::endl;
            strobe_hashval_next = string_hashes[i];
        }
    }
//    std::cerr << "Offset: " <<  strobe_pos_next - w_start << " val: " << min_val <<  ", P exact:" <<  1.0 - pow ( (float) (8-min_val)/9, strobe_pos_next - w_start) << std::endl;

}

void seq_to_randstrobes2(ind_mers_vector& flat_vector, int n, int k, int w_min, int w_max, std::string &seq, int ref_index, int s, int t, uint64_t q, int max_dist)
{

    if (seq.length() < w_max) {
        return;
    }

    std::transform(seq.begin(), seq.end(), seq.begin(), ::toupper);
    uint64_t kmask=(1ULL<<2*k) - 1;
    // make string of strobes into hashvalues all at once to avoid repetitive k-mer to hash value computations
    std::vector<uint64_t> string_hashes;
    std::vector<unsigned int> pos_to_seq_choord;
//    robin_hood::unordered_map< unsigned int, unsigned int>  pos_to_seq_choord;
//    make_string_to_hashvalues_random_minimizers(seq, string_hashes, pos_to_seq_choord, k, kmask, w);

//    int s = k-4;
//    int t = 3;
    uint64_t smask=(1ULL<<2*s) - 1;
    make_string_to_hashvalues_open_syncmers_canonical(seq, string_hashes, pos_to_seq_choord, kmask, k, smask, s, t);
//    make_string_to_hashvalues_open_syncmers(seq, string_hashes, pos_to_seq_choord, kmask, k, smask, s, t);

    unsigned int nr_hashes = string_hashes.size();
    if (nr_hashes == 0) {
        return;
    }

//        for (unsigned int i = 0; i < seq_length; i++) {
//        std::cerr << "REF POS INDEXED: " << pos_to_seq_choord[i] << " OTHER DIRECTION: " << seq.length() - pos_to_seq_choord[i] - k <<std::endl;
//    }
//    int tmp_cnt = 0;
//    for (auto a: pos_to_seq_choord){
//        std::cerr << " OK: " << tmp_cnt << " " << a << std::endl;
//        tmp_cnt ++;
//    }
//    std::cerr << seq << std::endl;

    // create the randstrobes
    for (unsigned int i = 0; i < nr_hashes; i++) {

//        if ((i % 1000000) == 0 ){
//            std::cerr << i << " strobemers created." << std::endl;
//        }
        unsigned int strobe_pos_next;
        uint64_t strobe_hashval_next;
        unsigned int seq_pos_strobe1 = pos_to_seq_choord[i];
        unsigned int seq_end = seq_pos_strobe1 + max_dist;

        if (i + w_max < nr_hashes){
            unsigned int w_start = i+w_min;
            unsigned int w_end = i+w_max;
            uint64_t strobe_hash;
            strobe_hash = string_hashes[i];
//            get_next_strobe(string_hashes, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q);
            get_next_strobe_dist_constraint(string_hashes, pos_to_seq_choord, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q, seq_pos_strobe1, seq_end,i);

        }
        else if (i + w_min + 1 < nr_hashes) {
            unsigned int w_start = i+w_min;
            unsigned int w_end = nr_hashes - 1;
            uint64_t strobe_hash;
            strobe_hash = string_hashes[i];
//            get_next_strobe(string_hashes, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q);
            get_next_strobe_dist_constraint(string_hashes, pos_to_seq_choord, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q, seq_pos_strobe1, seq_end, i);

        }
        else{
//            std::cerr << randstrobes2.size() << " randstrobes generated" << '\n';
            return;
        }

//        uint64_t hash_randstrobe2 = (string_hashes[i] << k) ^ strobe_hashval_next;
        uint64_t hash_randstrobe2 = (string_hashes[i]) + (strobe_hashval_next);


        unsigned int seq_pos_strobe2 = pos_to_seq_choord[strobe_pos_next];
//        std::cerr <<  "Seed length: " << seq_pos_strobe2 + k - seq_pos_strobe1 << std::endl;
        int packed = (ref_index << 8);
//        int offset_strobe =  seq_pos_strobe2 - seq_pos_strobe1;
        packed = packed + (seq_pos_strobe2 - seq_pos_strobe1);
        std::tuple<uint64_t, uint32_t, int32_t> s(hash_randstrobe2, seq_pos_strobe1, packed);
        flat_vector.push_back(s);
//        std::cerr << seq_pos_strobe1 << " " << seq_pos_strobe2 << std::endl;
//        std::cerr << "FORWARD REF: " << seq_pos_strobe1 << " " << seq_pos_strobe2 << " " << hash_randstrobe2 << std::endl;
//        std::cerr << "REFERENCE: " << seq_pos_strobe1 << " " << seq_pos_strobe2 << " " << hash_randstrobe2 << std::endl;


//        auto strobe1 = seq.substr(seq_pos_strobe1, k);
//        auto strobe2 = seq.substr(seq_pos_strobe2, k);
//        unsigned int seq_pos_strobe2 = pos_to_seq_choord[strobe_pos_next];
//        if (seq_pos_strobe2 > (seq_pos_strobe1+k)) {
////            std::cerr << seq_pos_strobe1 << " LOOOOL " << seq_pos_strobe2 << " " << seq_pos_strobe2 - (seq_pos_strobe1+k) << std::endl;
//            std::cerr << std::string(seq_pos_strobe1, ' ') << strobe1 << std::string(seq_pos_strobe2 - (seq_pos_strobe1+k), ' ') << strobe2 << std::endl;
//        }
//        std::cerr << i << " " << strobe_pos_next << " " << seq_pos_strobe1 << " " << seq_pos_strobe2 << " " << seq_pos_strobe2 - (seq_pos_strobe1+k) << " " << hash_randstrobe2 << std::endl;
//        std::cerr << i << " " << strobe_pos_next << std::endl;

    }
//    std::cerr << randstrobes2.size() << " randstrobes generated" << '\n';
}

mers_vector_read seq_to_randstrobes2_read(int n, int k, int w_min, int w_max, std::string& seq, unsigned int  ref_index, int s, int t, uint64_t q, int max_dist)
{
    // this function differs from  the function seq_to_randstrobes2 which creating randstrobes for the reference.
    // The seq_to_randstrobes2 stores randstobes only in one direction from canonical syncmers.
    // this function stores randstobes from both directions created from canonical syncmers.
    // Since creating canonical syncmers is the most time consuming step, we avoid perfomring it twice for the read and its RC here
    mers_vector_read randstrobes2;
    unsigned int read_length = seq.length();
    if (read_length < w_max) {
        return randstrobes2;
    }

    std::transform(seq.begin(), seq.end(), seq.begin(), ::toupper);
    uint64_t kmask=(1ULL<<2*k) - 1;
//    std::bitset<64> x(q);
//    std::cerr << x << '\n';
    // make string of strobes into hashvalues all at once to avoid repetitive k-mer to hash value computations
    std::vector<uint64_t> string_hashes;
    std::vector<unsigned int> pos_to_seq_choord;
//    robin_hood::unordered_map< unsigned int, unsigned int>  pos_to_seq_choord;
//    make_string_to_hashvalues_random_minimizers(seq, string_hashes, pos_to_seq_choord, k, kmask, w);

//    int t = 3;
    uint64_t smask=(1ULL<<2*s) - 1;
    make_string_to_hashvalues_open_syncmers_canonical(seq, string_hashes, pos_to_seq_choord, kmask, k, smask, s, t);
//    make_string_to_hashvalues_open_syncmers(seq, string_hashes, pos_to_seq_choord, kmask, k, smask, s, t);

    unsigned int nr_hashes = string_hashes.size();
//    std::cerr << nr_hashes << " okay" << std::endl;
    if (nr_hashes == 0) {
        return randstrobes2;
    }

//    int tmp_cnt = 0;
//    for (auto a: pos_to_seq_choord){
//        std::cerr << " OK: " << tmp_cnt << " " << a << std::endl;
//        tmp_cnt ++;
//    }
//    std::cerr << seq << std::endl;

    // create the randstrobes FW direction!
    for (unsigned int i = 0; i < nr_hashes; i++) {

//        if ((i % 1000000) == 0 ){
//            std::cerr << i << " strobemers created." << std::endl;
//        }
        unsigned int strobe_pos_next;
        uint64_t strobe_hashval_next;
        unsigned int seq_pos_strobe1 = pos_to_seq_choord[i];
        unsigned int seq_end = seq_pos_strobe1 + max_dist;
        if (i + w_max < nr_hashes){
            unsigned int w_start = i+w_min;
            unsigned int w_end = i+w_max;
            uint64_t strobe_hash;
            strobe_hash = string_hashes[i];
//            get_next_strobe(string_hashes, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q);
            get_next_strobe_dist_constraint(string_hashes, pos_to_seq_choord, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q, seq_pos_strobe1, seq_end, i);

        }
        else if (i + w_min + 1 < nr_hashes) {
            unsigned int w_start = i+w_min;
            unsigned int w_end = nr_hashes -1;
            uint64_t strobe_hash;
            strobe_hash = string_hashes[i];
//            get_next_strobe(string_hashes, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q);
            get_next_strobe_dist_constraint(string_hashes, pos_to_seq_choord, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q, seq_pos_strobe1, seq_end, i);
        }
        else{
//            std::cerr << randstrobes2.size() << " randstrobes generated" << '\n';
            break;
        }

//        uint64_t hash_randstrobe2 = (string_hashes[i] << k) ^ strobe_hashval_next;
        uint64_t hash_randstrobe2 = (string_hashes[i]) + (strobe_hashval_next);

        unsigned int seq_pos_strobe2 = pos_to_seq_choord[strobe_pos_next];
        unsigned int offset_strobe =  seq_pos_strobe2 - seq_pos_strobe1;
        std::tuple<uint64_t, unsigned int, unsigned int, unsigned int, bool> s (hash_randstrobe2, ref_index, seq_pos_strobe1, offset_strobe, false);
        randstrobes2.push_back(s);
//        std::cerr << "FORWARD: " << seq_pos_strobe1 << " " << seq_pos_strobe2 << " " << hash_randstrobe2 << std::endl;

//        auto strobe1 = seq.substr(seq_pos_strobe1, k);
//        unsigned int seq_pos_strobe2 = pos_to_seq_choord[strobe_pos_next];
//        if (seq_pos_strobe2 > (seq_pos_strobe1+k)) {
////            std::cerr << seq_pos_strobe1 << " LOOOOL " << seq_pos_strobe2 << " " << seq_pos_strobe2 - (seq_pos_strobe1+k) << std::endl;
//            std::cerr << std::string(seq_pos_strobe1, ' ') << strobe1 << std::string(seq_pos_strobe2 - (seq_pos_strobe1+k), ' ') << std::string(k, 'X') << std::endl;
//        }
//        std::cerr << i << " " << strobe_pos_next << " " << seq_pos_strobe1 << " " << seq_pos_strobe2 << " " << seq_pos_strobe2 - (seq_pos_strobe1+k) << " " << hash_randstrobe2 << std::endl;
//        std::cerr << i << " " << strobe_pos_next << std::endl;

    }

    // create the randstrobes Reverse direction!
    std::reverse(string_hashes.begin(), string_hashes.end());
    std::reverse(pos_to_seq_choord.begin(), pos_to_seq_choord.end());
    for (unsigned int i = 0; i < nr_hashes; i++) {
        pos_to_seq_choord[i] = read_length - pos_to_seq_choord[i] - k;
    }

//    for (unsigned int i = 0; i < nr_hashes; i++) {
//        std::cerr << "REVERSE: " << pos_to_seq_choord[i] <<std::endl;
//    }

    for (unsigned int i = 0; i < nr_hashes; i++) {

//        if ((i % 1000000) == 0 ){
//            std::cerr << i << " strobemers created." << std::endl;
//        }
        unsigned int strobe_pos_next;
        uint64_t strobe_hashval_next;
        unsigned int seq_pos_strobe1 = pos_to_seq_choord[i];
        unsigned int seq_end = seq_pos_strobe1 + max_dist;

        if (i + w_max < nr_hashes){
            unsigned int w_start = i+w_min;
            unsigned int w_end = i+w_max;
            uint64_t strobe_hash;
            strobe_hash = string_hashes[i];
//            get_next_strobe(string_hashes, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q);
            get_next_strobe_dist_constraint(string_hashes, pos_to_seq_choord, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q, seq_pos_strobe1, seq_end, i);

        }
        else if ((i + w_min + 1 < nr_hashes) && (nr_hashes <= i + w_max) ){
            unsigned int w_start = i+w_min;
            unsigned int w_end = nr_hashes -1;
            uint64_t strobe_hash;
            strobe_hash = string_hashes[i];
//            get_next_strobe(string_hashes, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q);
            get_next_strobe_dist_constraint(string_hashes, pos_to_seq_choord, strobe_hash, strobe_pos_next, strobe_hashval_next, w_start, w_end, q, seq_pos_strobe1, seq_end, i);

        }
        else{
//            std::cerr << randstrobes2.size() << " randstrobes generated" << '\n';
            return randstrobes2;
        }

//        uint64_t hash_randstrobe2 = (string_hashes[i] << k) ^ strobe_hashval_next;
        uint64_t hash_randstrobe2 = (string_hashes[i]) + (strobe_hashval_next);

        unsigned int seq_pos_strobe2 = pos_to_seq_choord[strobe_pos_next];
        unsigned int offset_strobe =  seq_pos_strobe2 - seq_pos_strobe1;
        std::tuple<uint64_t, unsigned int, unsigned int, unsigned int, bool> s (hash_randstrobe2, ref_index, seq_pos_strobe1, offset_strobe, true);
        randstrobes2.push_back(s);
//        std::cerr << "REVERSE: " << seq_pos_strobe1 << " " << seq_pos_strobe2 << " " << hash_randstrobe2 << std::endl;

//        auto strobe1 = seq.substr(seq_pos_strobe1, k);
//        unsigned int seq_pos_strobe2 = pos_to_seq_choord[strobe_pos_next];
//        if (seq_pos_strobe2 > (seq_pos_strobe1+k)) {
////            std::cerr << seq_pos_strobe1 << " LOOOOL " << seq_pos_strobe2 << " " << seq_pos_strobe2 - (seq_pos_strobe1+k) << std::endl;
//            std::cerr << std::string(seq_pos_strobe1, ' ') << strobe1 << std::string(seq_pos_strobe2 - (seq_pos_strobe1+k), ' ') << std::string(k, 'X') << std::endl;
//        }
//        std::cerr << i << " " << strobe_pos_next << " " << seq_pos_strobe1 << " " << seq_pos_strobe2 << " " << seq_pos_strobe2 - (seq_pos_strobe1+k) << " " << hash_randstrobe2 << std::endl;
//        std::cerr << i << " " << strobe_pos_next << std::endl;

    }
//    std::cerr << randstrobes2.size() << " randstrobes generated" << '\n';
    return randstrobes2;
}

//mers_vector seq_to_randstrobes3(int n, int k, int w_min, int w_max, std::string &seq, unsigned int ref_index, int w)
//{
//    mers_vector randstrobes3;
//
//    if (seq.length() < 2*w_max) {
//        return randstrobes3;
//    }
//
//    std::transform(seq.begin(), seq.end(), seq.begin(), ::toupper);
//    uint64_t kmask=(1ULL<<2*k) - 1;
//    uint64_t q = pow (2, 16) - 1;
//    // make string of strobes into hashvalues all at once to avoid repetitive k-mer to hash value computations
//    std::vector<uint64_t> string_hashes;
////    std::vector<uint64_t> pos_to_seq_choord;
//    std::vector<unsigned int> pos_to_seq_choord;
////    robin_hood::unordered_map< unsigned int, unsigned int>  pos_to_seq_choord;
//    make_string_to_hashvalues_random_minimizers(seq, string_hashes, pos_to_seq_choord, k, kmask, w);
//    unsigned int seq_length = string_hashes.size();
//
////    std::cerr << seq << std::endl;
//
//    // create the randstrobes
//    for (unsigned int i = 0; i <= seq_length; i++) {
//
////        if ((i % 10) == 0 ){
////            std::cerr << i << " randstrobes created." << std::endl;
////        }
//        uint64_t strobe_hash;
//        strobe_hash = string_hashes[i];
//
//        unsigned int strobe_pos_next1;
//        uint64_t strobe_hashval_next1;
//        unsigned int strobe_pos_next2;
//        uint64_t strobe_hashval_next2;
//
//        if (i + 2*w_max < seq_length){
//            unsigned int w1_start = i+w_min;
//            unsigned int w1_end = i+w_max;
//            get_next_strobe(string_hashes, strobe_hash, strobe_pos_next1, strobe_hashval_next1, w1_start, w1_end, q);
//
//            unsigned int w2_start = i+w_max + w_min;
//            unsigned int w2_end = i+2*w_max;
////            uint64_t conditional_next = strobe_hash ^ strobe_hashval_next1;
//            get_next_strobe(string_hashes, strobe_hashval_next1, strobe_pos_next2, strobe_hashval_next2, w2_start, w2_end, q);
//        }
//
//        else if ((i + 2*w_min + 1 < seq_length) && (seq_length <= i + 2*w_max) ){
//
//            int overshot;
//            overshot = i + 2*w_max - seq_length;
//            unsigned int w1_start = i+w_min;
//            unsigned int w1_end = i+w_max - overshot/2;
//            get_next_strobe(string_hashes, strobe_hash, strobe_pos_next1, strobe_hashval_next1, w1_start, w1_end, q);
//
//            unsigned int w2_start = i+w_max - overshot/2 + w_min;
//            unsigned int w2_end = i+2*w_max - overshot;
////            uint64_t conditional_next = strobe_hash ^ strobe_hashval_next1;
//            get_next_strobe(string_hashes, strobe_hashval_next1, strobe_pos_next2, strobe_hashval_next2, w2_start, w2_end, q);
//        }
//        else{
//            return randstrobes3;
//        }
//
//        uint64_t hash_randstrobe3 = (strobe_hash/3) + (strobe_hashval_next1/4) + (strobe_hashval_next2/5);
//
//        unsigned int seq_pos_strobe1 = pos_to_seq_choord[i];
////        unsigned int seq_pos_strobe2 =  pos_to_seq_choord[strobe_pos_next1]; //seq_pos_strobe1 + (strobe_pos_next1 - i); //
//        unsigned int seq_pos_strobe3 =  pos_to_seq_choord[strobe_pos_next2]; //seq_pos_strobe1 + (strobe_pos_next2 - i); //
////        std::cerr << i << " " << strobe_pos_next1 << " " << strobe_pos_next2 << " " << seq_pos_strobe1 << " " << seq_pos_strobe2 << " " << seq_pos_strobe3 << " " << pos_to_seq_choord.size() << std::endl;
//
////         TODO: Take care of corner case (tmep if statement below. Some values in end of string produce a cororidnate of 0 for the last strobe. Probably an off-by-one error in the calculation of the strobe coord in the last strobe window
////        if (strobe_pos_next2 ==  seq_length){
//////            std::cerr << "OMGGGGGGG " << i << " " << seq_pos_strobe1 << " " << seq_pos_strobe2 << " " << seq_pos_strobe3 << std::endl;
////            seq_pos_strobe3 = seq_length-1;
////        }
//        std::tuple<uint64_t, unsigned int, unsigned int, unsigned int> s (hash_randstrobe3, ref_index, seq_pos_strobe1, seq_pos_strobe3);
//        randstrobes3.push_back(s);
//
//
////        auto strobe1 = seq.substr(i, k);
////        std::cerr << std::string(i, ' ') << strobe1 << std::string(strobe_pos_next1 - (i+k), ' ') << std::string(k, 'X') << std::string(strobe_pos_next2 - strobe_pos_next1 - k, ' ') << std::string(k, 'X') << std::endl;
////        std::cerr << i << " " << strobe_pos_next1 << " " << strobe_pos_next2 << " " << seq_length << std::endl;
//    }
//    return randstrobes3;
//}













