#pragma once
#include <cmath>
#include <complex>

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
        float cos_start[4], sin_start[4];
        for (int i = 0; i < 4; i++) {
            cos_start[i] = cos(starting_phase);
            sin_start[i] = sin(starting_phase);
        }

        float const* pdcos = d_cos;
        float const* pdsin = d_sin;
        float const* pinput = (float const*)input;
        float const* pinput_end = (float const*)(input + input_size);
        float* poutput = (float*)output;

// Register map:
#define RDCOS "q0"  // dcos, dsin
#define RDSIN "q1"
#define RCOSST "q2"  // cos_start, sin_start
#define RSINST "q3"
#define RCOSV "q4"  // cos_vals, sin_vals
#define RSINV "q5"
#define ROUTI "q6"  // output_i, output_q
#define ROUTQ "q7"
#define RINPI "q8"  // input_i, input_q
#define RINPQ "q9"
#define R3(x, y, z) x ", " y ", " z "\n\t"

        asm volatile(  //(the range of q is q0-q15)
            "       vld1.32 {" RDCOS
            "}, [%[pdcos]]\n\t"
            "       vld1.32 {" RDSIN
            "}, [%[pdsin]]\n\t"
            "       vld1.32 {" RCOSST
            "}, [%[cos_start]]\n\t"
            "       vld1.32 {" RSINST
            "}, [%[sin_start]]\n\t"
            "for_addfast: vld2.32 {" RINPI "-" RINPQ
            "}, [%[pinput]]!\n\t"  // load q0 and q1 directly from the memory address stored in pinput, with interleaving (so that we get the I samples in RINPI and the Q samples in RINPQ), also
                                   // increment the memory address in pinput (hence the "!" mark)

            // C version:
            // cos_vals[j] = cos_start * d->dcos[j] - sin_start * d->dsin[j];
            // sin_vals[j] = sin_start * d->dcos[j] + cos_start * d->dsin[j];

            "       vmul.f32 " R3(RCOSV, RCOSST, RDCOS)  // cos_vals[i] = cos_start * d->dcos[i]
            "       vmls.f32 " R3(RCOSV, RSINST, RDSIN)  // cos_vals[i] -= sin_start * d->dsin[i]
            "       vmul.f32 " R3(RSINV, RSINST, RDCOS)  // sin_vals[i] = sin_start * d->dcos[i]
            "       vmla.f32 " R3(RSINV, RCOSST, RDSIN)  // sin_vals[i] += cos_start * d->dsin[i]

            // C version:
            // iof(output,4*i+j)=cos_vals[j]*iof(input,4*i+j)-sin_vals[j]*qof(input,4*i+j);
            // qof(output,4*i+j)=sin_vals[j]*iof(input,4*i+j)+cos_vals[j]*qof(input,4*i+j);
            "       vmul.f32 " R3(ROUTI, RCOSV, RINPI)  // output_i =  cos_vals * input_i
            "       vmls.f32 " R3(ROUTI, RSINV, RINPQ)  // output_i -= sin_vals * input_q
            "       vmul.f32 " R3(ROUTQ, RSINV, RINPI)  // output_q =  sin_vals * input_i
            "       vmla.f32 " R3(ROUTQ, RCOSV, RINPQ)  // output_i += cos_vals * input_q

            "       vst2.32 {" ROUTI "-" ROUTQ
            "}, [%[poutput]]!\n\t"  // store the outputs in memory
            //"     add %[poutput],%[poutput],#32\n\t"
            "       vdup.32 " RCOSST
            ", d9[1]\n\t"  // cos_start[0-3] = cos_vals[3]
            "       vdup.32 " RSINST
            ", d11[1]\n\t"  // sin_start[0-3] = sin_vals[3]

            "       cmp %[pinput], %[pinput_end]\n\t"         // if(pinput != pinput_end)
            "       bcc for_addfast\n\t"                      //    then goto for_addfast
            : [pinput] "+r"(pinput), [poutput] "+r"(poutput)  // output operand list -> C variables that we will change from ASM
            : [pinput_end] "r"(pinput_end), [pdcos] "r"(pdcos), [pdsin] "r"(pdsin), [sin_start] "r"(sin_start), [cos_start] "r"(cos_start)  // input operand list
            : "memory", "q0", "q1", "q2", "q4", "q5", "q6", "q7", "q8", "q9", "cc"                                                          // clobber list
        );
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
