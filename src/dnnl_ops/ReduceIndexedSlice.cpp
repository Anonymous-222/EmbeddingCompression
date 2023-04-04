
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
#include <type_traits>
#include <sys/time.h>
#include <map>

#include "dnnl.hpp"

#include "../common/c_runtime_api.h"
#include "dnnl_runtime.h"
using namespace dnnl;
using namespace std;

extern "C" int cpu_ReduceIndexedSlice(const DLArrayHandle in_indices,
                                      const DLArrayHandle in_values,
                                      DLArrayHandle out_indices,
                                      DLArrayHandle out_values) {
    map<int, vector<size_t>> idx2map;
    size_t dup_index_size = 1;
    for (int i = 0; i < in_indices->ndim; ++i) {
        dup_index_size *= in_indices->shape[i];
    }
    size_t width = in_values->shape[in_values->ndim - 1];
    const int *dup_index = (const int *)in_indices->data;
    const float *in_val = (const float *)in_values->data;
    int *out_ind = (int *)out_indices->data;
    float *out_val = (float *)out_values->data;
    for (size_t i = 0; i < dup_index_size; ++i) {
        idx2map[dup_index[i]].emplace_back(i);
        out_ind[i] = -1;
    }

    size_t index_size = idx2map.size();
    size_t num_all = index_size * width;

    vector<pair<int, vector<size_t>>> idx2vector(idx2map.begin(),
                                                 idx2map.end());

#pragma omp parallel for
    for (size_t i = 0; i < num_all; ++i)
        out_val[i] = 0;

#pragma omp parallel for
    for (size_t i = 0; i < index_size; ++i) {
        auto cur_pair = idx2vector[i];
        out_ind[i] = cur_pair.first;
        size_t cur_offset = i * width;
        for (auto j : cur_pair.second) {
            size_t ori_offset = j * width;
            for (size_t k = 0; k < width; ++k) {
                out_val[cur_offset + k] += in_val[ori_offset + k];
            }
        }
    }

    return 0;
}

extern "C" int cpu_ReduceIndexedSliceWithEmbedding(
    const DLArrayHandle in_indices, const DLArrayHandle in_values,
    const DLArrayHandle in_params, DLArrayHandle out_indices,
    DLArrayHandle out_values, DLArrayHandle out_params) {
    map<int, vector<size_t>> idx2map;
    size_t dup_index_size = 1;
    for (int i = 0; i < in_indices->ndim; ++i) {
        dup_index_size *= in_indices->shape[i];
    }
    size_t width = in_values->shape[in_values->ndim - 1];
    const int *dup_index = (const int *)in_indices->data;
    const float *in_val = (const float *)in_values->data;
    const float *in_par = (const float *)in_params->data;
    int *out_ind = (int *)out_indices->data;
    float *out_val = (float *)out_values->data;
    float *out_par = (float *)out_params->data;
    for (size_t i = 0; i < dup_index_size; ++i) {
        idx2map[dup_index[i]].emplace_back(i);
        out_ind[i] = -1;
    }

    size_t index_size = idx2map.size();
    size_t num_all = index_size * width;

    vector<pair<int, vector<size_t>>> idx2vector(idx2map.begin(),
                                                 idx2map.end());

#pragma omp parallel for
    for (size_t i = 0; i < num_all; ++i)
        out_val[i] = 0;

#pragma omp parallel for
    for (size_t i = 0; i < index_size; ++i) {
        auto cur_pair = idx2vector[i];
        out_ind[i] = cur_pair.first;
        size_t cur_offset = i * width;
        size_t ori_offset = cur_pair.second[0] * width;
        for (size_t k = 0; k < width; ++k) {
            out_par[cur_offset + k] = in_par[ori_offset + k];
        }
        for (auto j : cur_pair.second) {
            ori_offset = j * width;
            for (size_t k = 0; k < width; ++k) {
                out_val[cur_offset + k] += in_val[ori_offset + k];
            }
        }
    }

    return 0;
}

int cpu_AssignWithIndexedSlices(DLArrayHandle embedding,
                                const DLArrayHandle indices,
                                const DLArrayHandle values) {
    size_t index_size = 1;
    for (int i = 0; i < indices->ndim; ++i) {
        index_size *= indices->shape[i];
    }
    const int *ind_data = (const int *)indices->data;
    const float *val_data = (const float *)values->data;
    float *embed_data = (float *)embedding->data;
    assert(embedding->ndim == 2);
    int nrow = embedding->shape[0];
    size_t width = embedding->shape[1];

#pragma omp parallel for
    for (size_t i = 0; i < index_size; ++i) {
        int index = ind_data[i];
        if (index < 0 || index >= nrow)
            continue;
        size_t emb_offset = index * width;
        size_t val_offset = i * width;
        for (size_t k = 0; k < width; ++k) {
            embed_data[emb_offset + k] = val_data[val_offset + k];
        }
    }
    return 0;
}

int cpu_SGDUpdateIndexedSlices(const DLArrayHandle indices,
                               const DLArrayHandle grads, DLArrayHandle params,
                               float lr) {
    size_t index_size = 1;
    for (int i = 0; i < indices->ndim; ++i) {
        index_size *= indices->shape[i];
    }
    size_t width = grads->shape[grads->ndim - 1];
    const int *ind_data = (const int *)indices->data;
    const float *grad_data = (const float *)grads->data;
    float *param_data = (float *)params->data;

#pragma omp parallel for
    for (size_t i = 0; i < index_size; ++i) {
        if (ind_data[i] < 0)
            continue;
        size_t offset = i * width;
        for (size_t k = 0; k < width; ++k) {
            param_data[offset + k] -= lr * grad_data[offset + k];
        }
    }
    return 0;
}