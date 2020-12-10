// numeric.h
// 

#ifndef NUMERIC_H
#define NUMERIC_H 

#include "BPF.h"
#include "WavFile.h"
#include "FFT.h"

#include "core.h"

#include <vector>
#include <valarray>

// SUPPORT  -----------------------------------------------------------------------
void gen10 (const std::valarray<Real>& coeff, std::valarray<Real>& values) {
	for (unsigned i = 0; i < values.size () - 1; ++i) {
		values[i] = 0;
		for (unsigned j = 0; j < coeff.size (); ++j) {
			values[i] += coeff[j] * sin (2. * M_PI * (j + 1) * (float) i / values.size ());
		}
		values[i] /= coeff.size ();
	}
	values[values.size () - 1] = values[0]; // guard point
}
int next_pow2 (int n) {
    if (n == 0 || ceil(log2(n)) == floor(log2(n))) return n;
    int count = 0;
    while (n != 0) {
        n = n>>1;
        count = count + 1;
    }
    return (1 << count) ;
}
template <typename T>
void complexMultiplyReplace (
    const T* src1, const T* src2, T* dest, int num) {
    while (num--) {
        T r1 = *src1++;
        T r2 = *src2++;
        T i1 = *src1++;
        T i2 = *src2++;

        *dest++ = r1*r2 - i1*i2;
        *dest++ = r1*i2 + r2*i1;
    }
}
template <typename T>
void interleave (T* stereo, const T* l, const T* r, int n) {
	for (int i = 0; i < n; ++i) {
		stereo[2 * i] = l[i];
		stereo[2 * i + 1] = r[i];
	}
}
template <typename T>
void deinterleave (const T* stereo, T* l, T* r, int n) {
	for (int i = 0; i < n / 2; ++i) {
		l[i] = stereo[2 * i];
		r[i] = stereo[2 * i + 1];
	}
}	
// NUMERIC --------------------------------------------------------------------------------------
AtomPtr fn_bpf (AtomPtr node, AtomPtr env) {
	Real init = type_check (node->sequence.at (0), AtomType::ARRAY, node)->array[0];
	int len  = (int) type_check (node->sequence.at (1), AtomType::ARRAY, node)->array[0];
	Real end = type_check (node->sequence.at (2), AtomType::ARRAY, node)->array[0];
	node->sequence.pop_front (); node->sequence.pop_front (); node->sequence.pop_front ();
	if (node->sequence.size () % 2 != 0) error ("invalid number of arguments for bpf", node);
	BPF<Real> bpf (len);
	bpf.add_segment (init, len, end);
	Real curr = end;
	for (unsigned i = 0; i < node->sequence.size () / 2; ++i) {
		int len  = (int) type_check (node->sequence.at (i * 2), AtomType::ARRAY, node)->array[0];
		Real end = type_check (node->sequence.at (i * 2 + 1), AtomType::ARRAY, node)->array[0];
		bpf.add_segment (curr, len, end);
		curr = end;
	}
	std::valarray<Real> out;
	bpf.process (out);
	return Atom::make_array (out);
}
AtomPtr fn_mix (AtomPtr node, AtomPtr env) {
	std::vector<Real> out;
	if (node->sequence.size () % 2 != 0) error ("invalid number of arguments for mix", node);
	for (unsigned i = 0; i < node->sequence.size () / 2; ++i) {
		int p = (int) type_check (node->sequence.at (i * 2), AtomType::ARRAY, node)->array[0];
		AtomPtr l = type_check (node->sequence.at (i * 2 + 1), AtomType::ARRAY, node);
		int len = (int) (p + l->array.size ());
		if (len > out.size ()) out.resize (len, 0);
		// out[std::slice(p, len, 1)] += l->array;
		for (unsigned t = 0; t < l->array.size (); ++t) {
			out[t + p] += l->array[t];
		}
	}
	std::valarray<Real> v (out.data(), out.size());
	return Atom::make_array (v);
}
AtomPtr fn_gen (AtomPtr node, AtomPtr env) {
	int len = (int) type_check (node->sequence.at (0), AtomType::ARRAY, node)->array[0];
	std::valarray<Real> coeffs = type_check (node->sequence.at (1), AtomType::ARRAY, node)->array;
	std::valarray<Real> table (len + 1); 
	gen10 (coeffs, table);
	return Atom::make_array (table);
}
AtomPtr fn_osc (AtomPtr node, AtomPtr env) {
	Real sr = type_check (node->sequence.at (0), AtomType::ARRAY, node)->array[0];
	std::valarray<Real> freqs = type_check (node->sequence.at (1), AtomType::ARRAY, node)->array;
	std::valarray<Real> table = type_check (node->sequence.at (2), AtomType::ARRAY, node)->array;
	std::valarray<Real> out (freqs.size ());
	int N = table.size () - 1;
	Real fn = (Real) sr / N; // Hz
	Real phi = 0; //rand () % (N - 1);
	for (unsigned i = 0; i < freqs.size (); ++i) {
		int intphi = (int) phi;
		Real fracphi = phi - intphi;
		Real c = (1 - fracphi) * table[intphi] + fracphi * table[intphi + 1];
		out[i] = c;
		phi = phi + freqs[i] / fn;
		if (phi >= N) phi = phi - N;
	}
	return Atom::make_array (out);
}
template <int sign>
AtomPtr fn_fft (AtomPtr n, AtomPtr env) {
	int d = type_check (n->sequence.at (0), AtomType::ARRAY, n)->array.size ();
	int N = next_pow2 (d);
	int norm = (sign < 0 ? 1 : N / 2);
    std::valarray<Real> inout (N);
	for (unsigned i = 0; i < d; ++i) inout[i] = n->sequence.at(0)->array[i];
    fft<Real> (&inout[0], N / 2, sign);
  	
	for (unsigned i = 0; i < N; ++i) inout[i] /= norm;	
	return Atom::make_array (inout);
}
AtomPtr fn_car2pol (AtomPtr n, AtomPtr env) {
	std::valarray<Real> inout = type_check (n->sequence.at (0), AtomType::ARRAY, n)->array;
	rect2pol (&inout[0], inout.size () / 2);
	return Atom::make_array (inout);
}
AtomPtr fn_pol2car (AtomPtr n, AtomPtr env) {
 	std::valarray<Real> inout = type_check (n->sequence.at (0), AtomType::ARRAY, n)->array;
	pol2rect (&inout[0], inout.size () / 2);
	return Atom::make_array (inout);
}
AtomPtr fn_conv (AtomPtr n, AtomPtr env) {
    std::valarray<Real> ir = type_check (n->sequence.at (0), AtomType::ARRAY, n)->array;
    std::valarray<Real> sig = type_check (n->sequence.at (1), AtomType::ARRAY, n)->array;
    Real scale = type_check(n->sequence.at (2), AtomType::ARRAY, n)->array[0];
	Real mix = 0;
	if (n->sequence.size () == 4) mix = type_check(n->sequence.at (3), AtomType::ARRAY, n)->array[0];
    long irsamps = ir.size ();
    long sigsamps = sig.size ();
    if (irsamps <= 0 || sigsamps <= 0) error ("invalid lengths for conv", n);
    int max = irsamps > sigsamps ? irsamps : sigsamps;
    int N = next_pow2(max) << 1;
	std::valarray<Real> fbuffir(2 * N);
	std::valarray<Real> fbuffsig(2 * N);
	std::valarray<Real> fbuffconv(2 * N);
    for (unsigned i = 0; i < N; ++i) {
        if (i < irsamps) fbuffir[2 * i] = ir[i];
        else fbuffir[2 * i] = 0;
        if (i < sigsamps) fbuffsig[2 * i] = sig[i];
        else fbuffsig[2 * i] = 0;
        fbuffconv[2 * i] = 0;
        fbuffir[2 * i + 1] = 0;
        fbuffsig[2 * i + 1] = 0;
        fbuffconv[2 * i + 1] = 0;        
    }
    AbstractFFT<Real>* fft = createFFT<Real>(N);
    fft->forward(&fbuffir[0]);
    fft->forward(&fbuffsig[0]);
    complexMultiplyReplace(&fbuffir[0], &fbuffsig[0], &fbuffconv[0], N);
    fft->inverse(&fbuffconv[0]);
	std::valarray<Real> out  (irsamps + sigsamps - 1);
    for (unsigned i = 0; i < (irsamps + sigsamps) -1; ++i) {
        Real s = scale * fbuffconv[2 * i] / N;
        if (i < sigsamps) s+= sig[i] * mix;
        out[i] = s;
    }
    return Atom::make_array (out);
}
// // I/O  -----------------------------------------------------------------------
AtomPtr fn_sndwrite (AtomPtr node, AtomPtr env) {
	Real sr = type_check (node->sequence.at (0), AtomType::ARRAY, node)->array[0];
	std::valarray<Real> vals;
	if (node->sequence.size () == 3) {
		WavOutFile outf (type_check (node->sequence.at (1), AtomType::STRING, node)->token.c_str(), sr, 16, 1);
		vals = type_check (node->sequence.at (2), AtomType::ARRAY, node)->array;
		outf.write (&vals[0], vals.size ());
	} else if (node->sequence.size () == 4) {
		WavOutFile outf (type_check (node->sequence.at (1), AtomType::STRING, node)->token.c_str(), sr, 16, 2);
		std::valarray<Real> left = type_check (node->sequence.at (2), AtomType::ARRAY, node)->array;
		std::valarray<Real> right = type_check (node->sequence.at (3), AtomType::ARRAY, node)->array;
		vals.resize (2 * left.size ());
		interleave (&vals[0], &left[0], &right[0], left.size ());
		outf.write (&vals[0], vals.size ());
	} else error ("invalid number of channels in", node->sequence.at(0));
	
	return Atom::make_array (vals.size ());
}
AtomPtr fn_sndread (AtomPtr node, AtomPtr env) {
	WavInFile infile (type_check (node->sequence.at (0), AtomType::STRING, node)->token.c_str());
	AtomPtr l = Atom::make_sequence ();
	int s = infile.getNumSamples ();
	std::valarray<Real> input (s);
	infile.read (&input[0], s);
	std::valarray<Real> info (3);
	info[0] = infile.getSampleRate ();
	info[1] = infile.getNumChannels ();
	info[2] = s;
	l->sequence.push_back (Atom::make_array (info));
	if (infile.getNumChannels () == 1) {
		l->sequence.push_back (Atom::make_array (input));
	} else if (infile.getNumChannels () == 2) {
		std::valarray<Real> left (s/2);
		std::valarray<Real> right (s/2);
		deinterleave (&input[0], &left[0], &right[0], s);
		l->sequence.push_back (Atom::make_array (left));
		l->sequence.push_back (Atom::make_array (right));
	} else error ("invalid number of channels in", node->sequence.at (0));
	return l;
}
void add_numeric (AtomPtr env) {
	add_builtin ("bpf", fn_bpf, 3, env);
	add_builtin ("mix", fn_mix, 2, env);	
	add_builtin ("gen", fn_gen, 2, env);
	add_builtin ("osc", fn_osc, 3, env);
	add_builtin ("fft", fn_fft<1>, 1, env);
	add_builtin ("ifft", fn_fft<-1>, 1, env);
	add_builtin ("car2pol", fn_car2pol, 1, env);
	add_builtin ("pol2car", fn_pol2car, 1, env);
	add_builtin ("conv", fn_conv, 3, env);
	// // I/O
	add_builtin ("sndwrite", fn_sndwrite, 3, env);
	add_builtin ("sndread", fn_sndread, 1, env);
}
#endif	// NUMERIC_H 

// EOF
