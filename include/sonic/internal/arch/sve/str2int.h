#pragma once

#define STR2INT_LOWEST_VALUE_MUL_FACTOR 10
#define STR2INT_CASE_TEN_MUL_FACTOR 100
#define STR2INT_CASE_ELEVEN_MUL_FACTOR 1000
#define STR2INT_CASE_TWELEVE_MUL_FACTOR 10000
#define STR2INT_CASE_THIRTEEN_MUL_FACTOR 100000
#define STR2INT_CASE_FOURTEEN_MUL_FACTOR 1000000
#define STR2INT_CASE_FIFTEEN_MUL_FACTOR 10000000
#define STR2INT_CASE_SIXTEEN_MUL_FACTOR 100000000

namespace sonic_json {
namespace internal {
namespace sve {
sonic_force_inline uint32_t low_half_simd_str2int(svuint32_t data, svbool_t curPg, uint32_t *mulFactor) {
    svuint32_t wideData = svrev_u32(data);
    wideData = svcompact_u32(curPg, wideData);
    svuint32_t vecFactor = svld1_u32(svptrue_pat_b32(SV_VL8), mulFactor);
    uint32_t lowSum = svaddv_u32(svptrue_pat_b32(SV_VL8), svmul_u32_x(svptrue_pat_b32(SV_VL8), vecFactor, wideData));
    return lowSum;
}

sonic_force_inline uint32_t low_full_half_simd_str2int(svuint32_t data, svbool_t curPg, uint32_t *mulFactor) {
    svuint32_t wideData = svrev_u32(data);
    svuint32_t vecFactor = svld1_u32(curPg, mulFactor);
    uint32_t lowSum = svaddv_u32(curPg, svmul_u32_x(curPg, vecFactor, wideData));
    return lowSum;
}

sonic_force_inline uint64_t process_low_case_wide_data(svuint16_t data, uint32_t *mulFactor, uint32_t num) {
    svbool_t curPg = svnot_z(svptrue_pat_b16(SV_VL16), svwhilelt_b32_u32(0, num));
    svuint32_t wideData = svunpklo_u32(data);
    return low_half_simd_str2int(wideData, curPg, mulFactor);
}

sonic_force_inline uint64_t process_low_case_8_data(svuint16_t data, uint32_t *mulFactor) {
    svbool_t pgWide = svptrue_pat_b32(SV_VL8);
    svuint32_t wideData = svunpklo_u32(data);
    return low_full_half_simd_str2int(wideData, pgWide, mulFactor);
}

sonic_force_inline uint64_t process_low_case_9_data(svuint16_t data, uint32_t *mulFactor) {
    svuint32_t wideData = svunpklo_u32(data);
    uint64_t lowSum = low_full_half_simd_str2int(wideData, svptrue_pat_b32(SV_VL8), mulFactor);
    svbool_t curPg = svwhilelt_b16_u32(0, 0x9);
    return svlastb_u16(curPg, data) + lowSum * STR2INT_LOWEST_VALUE_MUL_FACTOR;
}

sonic_force_inline uint64_t process_low_case_10_data(svuint16_t data, uint32_t *mulFactor) {
    svuint32_t wideData = svunpklo_u32(data);
    uint64_t lowSum = low_full_half_simd_str2int(wideData, svptrue_pat_b32(SV_VL8), mulFactor);
    svbool_t curPg = svwhilelt_b16_u32(0, 0x9);
    return svlasta_u16(curPg, data) + svlastb_u16(curPg, data) * STR2INT_LOWEST_VALUE_MUL_FACTOR +
        lowSum * STR2INT_CASE_TEN_MUL_FACTOR;
}

sonic_force_inline uint64_t process_high_case_wide_data(svuint16_t data, uint32_t *mulFactor, uint32_t num1,
    uint32_t num2) {
    svbool_t pgWide = svnot_z(svptrue_pat_b16(SV_VL16), svwhilelt_b32_u32(0, num1));
    svuint32_t wideData = svunpklo_u32(data);
    uint64_t lowSum = low_full_half_simd_str2int(wideData, svptrue_pat_b32(SV_VL8), mulFactor);
    wideData = svunpkhi_u32(data);
    uint64_t highSum = low_half_simd_str2int(wideData, pgWide, mulFactor);
    return lowSum * num2 + highSum;
}

sonic_force_inline uint64_t process_low_case_16_data(svuint16_t data, uint32_t *mulFactor) {
    svuint32_t wideData = svunpklo_u32(data);
    uint64_t lowSum = low_full_half_simd_str2int(wideData, svptrue_pat_b32(SV_VL8), mulFactor);
    wideData = svunpkhi_u32(data);
    uint64_t highSum = low_full_half_simd_str2int(wideData, svptrue_pat_b32(SV_VL8), mulFactor);
    return lowSum * STR2INT_CASE_SIXTEEN_MUL_FACTOR + highSum;
}

sonic_force_inline uint64_t simd_str2int(const char *c, int &man_nd) {
    uint32_t mulFactor[8] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};
    svbool_t pgAll = svptrue_pat_b16(SV_VL16);
    svuint16_t data = svld1sb_u16(pgAll, reinterpret_cast<const signed char *>(&c[0]));
    data = svsub_n_u16_x(pgAll, data, '0');
    svbool_t gt_nine = svcmpgt_n_u16(pgAll, data, 9);
    int num_end_idx = 16;
    if (svptest_any(pgAll, gt_nine)) {
        num_end_idx = svcntp_b16(pgAll, svbrkb_z(pgAll, gt_nine));
    }
    man_nd = man_nd < num_end_idx ? man_nd : num_end_idx;
    switch (man_nd) {
        case 1:
            return svlastb_u16(svwhilelt_b16_u32(0, 1), data);
        case 0x2:
            return svlastb_u16(svwhilelt_b16_u32(0, 1), data) * 0xa + svlasta_u16(svwhilelt_b16_u32(0, 1), data);
        case 0x3:
            return process_low_case_wide_data(data, mulFactor, 0x5);
        case 0x4:
            return process_low_case_wide_data(data, mulFactor, 0x4);
        case 0x5:
            return process_low_case_wide_data(data, mulFactor, 0x3);
        case 0x6:
            return process_low_case_wide_data(data, mulFactor, 0x2);
        case 0x7:
            return process_low_case_wide_data(data, mulFactor, 0x1);
        case 0x8:
            return process_low_case_8_data(data, mulFactor);
        case 0x9:
            return process_low_case_9_data(data, mulFactor);
        case 0xa:
            return process_low_case_10_data(data, mulFactor);
        case 0xb:
            return process_high_case_wide_data(data, mulFactor, 0x5, STR2INT_CASE_ELEVEN_MUL_FACTOR);
        case 0xc:
            return process_high_case_wide_data(data, mulFactor, 0x4, STR2INT_CASE_TWELEVE_MUL_FACTOR);
        case 0xd:
            return process_high_case_wide_data(data, mulFactor, 0x3, STR2INT_CASE_THIRTEEN_MUL_FACTOR);
        case 0xe:
            return process_high_case_wide_data(data, mulFactor, 0x2, STR2INT_CASE_FOURTEEN_MUL_FACTOR);
        case 0xf:
            return process_high_case_wide_data(data, mulFactor, 1, STR2INT_CASE_FIFTEEN_MUL_FACTOR);
        case 0x10:
            return process_low_case_16_data(data, mulFactor);
        default:
            return 0;
    }
    return 1;
}

} // namespace sve
} // namespace internal
} // namespace sonic_json
