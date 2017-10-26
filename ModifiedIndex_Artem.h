#pragma once

typedef unsigned int idx_t;
typedef unsigned char uint8_t;

namespace hnswlib {

    void write_pq(const char *path, faiss::ProductQuantizer *pq)
    {
        FILE *fout = fopen(path, "wb");

        fwrite(&pq->d, sizeof(size_t), 1, fout);      /** Save Vector Dimension **/
        fwrite(&pq->M, sizeof(size_t), 1, fout);      /** Save Number of Subquantizers **/
        fwrite(&pq->nbits, sizeof(size_t), 1, fout);  /** Save nbits: ksub = 1 << nbits **/

        /** Save Number of Centroids **/
        size_t size = pq->centroids.size();
        fwrite (&size, sizeof(size_t), 1, fout);

        /** Save Centroids **/
        const float *centroids = pq->centroids.data();
        fwrite(centroids, sizeof(float), size, fout);

        fclose(fout);
    }

    void read_pq(const char *path, faiss::ProductQuantizer *pq)
    {
        FILE *fin = fopen(path, "rb");

        fread(&pq->d, sizeof(size_t), 1, fin);
        fread(&pq->M, sizeof(size_t), 1, fin);
        fread(&pq->nbits, sizeof(size_t), 1, fin);
        pq->set_derived_values ();

        size_t size;
        fread (&size, sizeof(size_t), 1, fin);
        pq->centroids.reserve(size);

        float *centroids = pq->centroids.data();
        fread(centroids, sizeof(float), size, fin);

        fclose(fin);
    }


    class ModifiedIndex
	{
        Index *index;

		size_t d;             /** Vector Dimension **/
		size_t nc;            /** Number of Centroids **/
        size_t nsubc;         /** Number of Subcentroids **/
        size_t code_size;     /** PQ Code Size **/

        /** Search Parameters **/
        size_t nprobe = 16;
        size_t max_codes = 10000;

        std::vector < std::vector < std::vector<idx_t> > > ids;
        std::vector < std::vector < std::vector<uint8_t> > > codes;
        std::vector < std::vector < std::vector<uint8_t> > > norm_codes;

        std::vector < std::vector < idx_t > > nn_centroid_idxs;
        std::vector < float > alphas;

    public:
		ModifiedIndex(Index *trained_index, size_t nsubcentroids = 128):
                index(trained_index), nsubc(nsubcentroids)
		{
            nc = index->csize;
            d = index->d;
            code_size = index->code_size;

            codes.reserve(nc);
            norm_codes.reserve(nc);
            ids.reserve(nc);
            alphas.reserve(nc);
            nn_centroid_idxs.reserve(nc);

            for (int i = 0; i < nc; i++){
                ids[i].reserve(nsubc);
                codes[i].reserve(nsubc);
                norm_codes[i].reserve(nsubc);
                nn_centroid_idxs[i].reserve(nsubc);
            }
        }

		~ModifiedIndex() {}

        void write(const char *path_index) const
        {
            FILE *fout = fopen(path_index, "wb");

            fwrite(&d, sizeof(size_t), 1, fout);
            fwrite(&nc, sizeof(size_t), 1, fout);
            fwrite(&nsubc, sizeof(size_t), 1, fout);

            int size;
            /** Write Vector Indexes **/
            for (size_t i = 0; i < nc; i++)
                for (size_t j = 0; j < nsubc; j++) {
                    size = ids[i][j].size();
                    fwrite(&size, sizeof(int), 1, fout);
                    fwrite(ids[i][j].data(), sizeof(idx_t), size, fout);
                }

            /** Write PQ Codes **/
            for(size_t i = 0; i < nc; i++)
                for (size_t j = 0; j < nsubc; j++) {
                    size = codes[i][j].size();
                    fwrite(&size, sizeof(int), 1, fout);
                    fwrite(codes[i][j].data(), sizeof(uint8_t), size, fout);
                }

            /** Write Norm Codes **/
            for(size_t i = 0; i < nc; i++)
                for (size_t j = 0; j < nsubc; j++) {
                    size = norm_codes[i][j].size();
                    fwrite(&size, sizeof(int), 1, fout);
                    fwrite(norm_codes[i][j].data(), sizeof(uint8_t), size, fout);
                }

            /** Write NN Centroid Indexes **/
            for(size_t i = 0; i < nc; i++) {
                size = nn_centroid_idxs[i].size();
                fwrite(&size, sizeof(int), 1, fout);
                fwrite(nn_centroid_idxs[i].data(), sizeof(idx_t), size, fout);
            }

            /** Write Alphas **/
            fwrite(alphas.data(), sizeof(float), nc, fout);

            fclose(fout);
        }

        void read(const char *path_index)
        {
            FILE *fin = fopen(path_index, "rb");

            fread(&d, sizeof(size_t), 1, fin);
            fread(&nc, sizeof(size_t), 1, fin);
            fread(&nsubc, sizeof(size_t), 1, fin);

            ids.reserve(nc);
            codes.reserve(nc);
            norm_codes.reserve(nc);
            alphas.reserve(nc);
            nn_centroid_idxs.reserve(nc);

            for (int i = 0; i < nc; i++){
                ids[i].reserve(nsubc);
                codes[i].reserve(nsubc);
                norm_codes[i].reserve(nsubc);
            }

            idx_t size;
            for (size_t i = 0; i < nc; i++)
                for (size_t j = 0; j < nsubc; j++) {
                    fread(&size, sizeof(int), 1, fin);
                    ids[i][j].reserve(size);
                    fread(ids[i][j].data(), sizeof(idx_t), size, fin);
                }

            for(size_t i = 0; i < nc; i++)
                for (size_t j = 0; j < nsubc; j++) {
                    fread(&size, sizeof(idx_t), 1, fin);
                    codes[i][j].reserve(size);
                    fread(codes[i][j].data(), sizeof(uint8_t), size, fin);
                }

            for(size_t i = 0; i < nc; i++)
                for (size_t j = 0; j < nsubc; j++) {
                    fread(&size, sizeof(int), 1, fin);
                    norm_codes[i][j].reserve(size);
                    fread(norm_codes[i][j].data(), sizeof(uint8_t), size, fin);
                }


            for(int i = 0; i < nc; i++) {
                fread(&size, sizeof(int), 1, fin);
                nn_centroid_idxs[i].reserve(size);
                fread(nn_centroid_idxs[i].data(), sizeof(idx_t), size, fin);
            }

            fread(alphas.data(), sizeof(float), nc, fin);

            fclose(fin);
        }
	};

}
