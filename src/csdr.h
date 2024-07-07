#pragma once
#include <cmath>
#include <complex>

#define iof_l(complexf_input_p, i) (*(((float*)complexf_input_p) + 2 * (i)))
#define qof_l(complexf_input_p, i) (*(((float*)complexf_input_p) + 2 * (i) + 1))
#define iof(complexf_input_p, i) (*(((float const*)complexf_input_p) + 2 * (i)))
#define qof(complexf_input_p, i) (*(((float const*)complexf_input_p) + 2 * (i) + 1))
#define SADF_L1(j)                                              \
    cos_vals_##j = cos_start * dcos_##j - sin_start * dsin_##j; \
    sin_vals_##j = sin_start * dcos_##j + cos_start * dsin_##j;
#define SADF_L2(j)                                                                                              \
    iof_l(output, 4 * i + j) = (cos_vals_##j) * iof(input, 4 * i + j) - (sin_vals_##j) * qof(input, 4 * i + j); \
    qof_l(output, 4 * i + j) = (sin_vals_##j) * iof(input, 4 * i + j) + (cos_vals_##j) * qof(input, 4 * i + j);

struct freq_shift {
   private:
    float d_sin[4];
    float d_cos[4];
    float phase_increment;

   public:
    freq_shift(float rate) {
        phase_increment = 2 * rate * M_PI;
        for (auto i = 0; i < 4; i++) {
            d_sin[i] = sin(phase_increment * (i + 1));
            d_cos[i] = cos(phase_increment * (i + 1));
        }
    }

    float shift_addfast_cc(std::complex<float> const* input, std::complex<float>* output, int input_size, float starting_phase) const {
        // input_size should be multiple of 4
        // fprintf(stderr, "shift_addfast_cc: input_size = %d\n", input_size);
        float cos_start = cos(starting_phase);
        float sin_start = sin(starting_phase);
        float cos_vals_0, cos_vals_1, cos_vals_2, cos_vals_3, sin_vals_0, sin_vals_1, sin_vals_2, sin_vals_3;
        float dsin_0 = d_sin[0];
        float dsin_1 = d_sin[1];
        float dsin_2 = d_sin[2];
        float dsin_3 = d_sin[3];
        float dcos_0 = d_cos[0];
        float dcos_1 = d_cos[1];
        float dcos_2 = d_cos[2];
        float dcos_3 = d_cos[3];

        for (int i = 0; i < input_size / 4; ++i)  //@shift_addfast_cc
        {
            SADF_L1(0)
            SADF_L1(1)
            SADF_L1(2)
            SADF_L1(3)
            SADF_L2(0)
            SADF_L2(1)
            SADF_L2(2)
            SADF_L2(3)
            cos_start = cos_vals_3;
            sin_start = sin_vals_3;
        }
        starting_phase += input_size * phase_increment;
        while (starting_phase > M_PI)
            starting_phase -= 2 * M_PI;
        while (starting_phase < -M_PI)
            starting_phase += 2 * M_PI;
        return starting_phase;
    }

    void operator()(float const* in, float* out, int len) const {
        auto starting_phase = 0.0f;
        auto const* iptr = in;
        auto* optr = out;
        while (len) {
            auto count = std::min(1024, len);
            starting_phase = shift_addfast_cc((std::complex<float> const*)iptr, (std::complex<float>*)optr, count, starting_phase);
            iptr += count * 2;
            optr += count * 2;
            len -= count;
        }
    }
};
