// numeric.h
// 

#ifndef NUMERIC_H
#define NUMERIC_H 

#include "BPF.h"
#include "WavFile.h"
#include "FFT.h"

#include "core.h"

// SUPPORT  -----------------------------------------------------------------------
void gen10 (const std::vector<Real>& coeff, std::vector<Real>& values) {
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
SexprPtr list_from_vector (const std::vector<Real>& inv) {
	SexprPtr l = Sexpr::make_list ();
	for (unsigned i = 0; i < inv.size (); ++i) l->tail.push_back (Sexpr::make_number (inv.at (i)));
	return l;
}
std::vector<Real> vector_from_list (SexprPtr l) {
	std::vector<Real> outv;
	for (unsigned i = 0; i < l->tail.size (); ++i) outv.push_back (type_check (l->tail.at (i), SexprType::NUMBER)->value);
	return outv;
}
// VECTORS -----------------------------------------------------------------------
#define MAKE_VECBINOP(op,name)	\
	SexprPtr name (SexprPtr n, SexprPtr env) {	\
		std::vector<Real> c = vector_from_list (type_check (n->tail.at (0), SexprType::LIST)); \
		if (c.size () < 1) return Sexpr::make_list (); \
		for (unsigned i = 1; i < n->tail.size (); ++i) {	\
			for (unsigned k = 0; k < c.size (); ++k) { \
				SexprPtr b = n->tail.at (i);\
				if (b->tail.size () < 1) break; \
				if (c.size () != b->tail.size ()) error ("incompatible list size", n);\
				c.at (k) = c.at (k) op type_check (b->tail.at(k), SexprType::NUMBER)->value; \
			} \
		}\
		return list_from_vector (c);	\
	} \

MAKE_VECBINOP (+, addv_fn);
MAKE_VECBINOP (-, subv_fn);
MAKE_VECBINOP (*, mulv_fn);
MAKE_VECBINOP (/, divv_fn);

#define MAKE_VECMINMAX(op,name)	\
SexprPtr name (SexprPtr n, SexprPtr env) { \
	std::vector<Real> c = vector_from_list (type_check (n->tail.at (0), SexprType::LIST)); \
	if (c.size () < 1) return Sexpr::make_list (); \
	Real m = c.at (0); \
	for (unsigned i = 1; i < c.size (); ++i) { \
		if (c.at (i) op m) m = c.at (i); \
	} \
	return Sexpr::make_number(m); \
} \

MAKE_VECMINMAX (<, minv_fn);
MAKE_VECMINMAX (>, maxv_fn);

#define MAKE_VECSUMMEAN(name,is_mean) \
SexprPtr name (SexprPtr n, SexprPtr env) { \
	std::vector<Real> c = vector_from_list (type_check (n->tail.at (0), SexprType::LIST)); \
	if (c.size () < 1) return Sexpr::make_list (); \
	Real m = c.at (0); \
	for (unsigned i = 1; i < c.size (); ++i) { \
		m += c.at (i); \
	} \
	if (is_mean) m /= c.size (); \
	return Sexpr::make_number(m);  \
} \

MAKE_VECSUMMEAN (sumv_fn, 0);
MAKE_VECSUMMEAN (mean_fn, 1);

#define MAKE_VECSINGOP(op, name) \
SexprPtr name (SexprPtr n, SexprPtr env) { \
	std::vector<Real> c = vector_from_list (type_check (n->tail.at (0), SexprType::LIST)); \
	if (c.size () < 1) return Sexpr::make_list (); \
	for (unsigned i = 0; i < c.size (); ++i) { \
		Real m = c.at (i); \
		c.at(i) = op (m); \
	} \
	return list_from_vector (c);  \
} \

MAKE_VECSINGOP (sin, sinv_fn);
MAKE_VECSINGOP (cos, cosv_fn);
MAKE_VECSINGOP (exp, expv_fn);
MAKE_VECSINGOP (log, logv_fn);
MAKE_VECSINGOP (sqrt, sqrtv_fn);
MAKE_VECSINGOP (fabs, absv_fn);
MAKE_VECSINGOP (floor, floorv_fn);

SexprPtr bpf_fn (SexprPtr node, SexprPtr env) {
	Real init = type_check (node->tail.at (0), SexprType::NUMBER)->value;
	int len  = (int) type_check (node->tail.at (1), SexprType::NUMBER)->value;
	Real end = type_check (node->tail.at (2), SexprType::NUMBER)->value;
	node->tail.pop_front (); node->tail.pop_front (); node->tail.pop_front ();
	if (node->tail.size () % 2 != 0) error ("invalid number of arguments", node);
	BPF<Real> bpf (len);
	bpf.add_segment (init, len, end);
	Real curr = end;
	for (unsigned i = 0; i < node->tail.size () / 2; ++i) {
		int len  = (int) type_check (node->tail.at (i * 2), SexprType::NUMBER)->value;
		Real end = type_check (node->tail.at (i * 2 + 1), SexprType::NUMBER)->value;
		bpf.add_segment (curr, len, end);
		curr = end;
	}
	std::vector<Real> out;
	bpf.process (out);
	return list_from_vector (out);
}
SexprPtr mix_fn (SexprPtr node, SexprPtr env) {
	std::vector<Real> out;
	if (node->tail.size () % 2 != 0) error ("invalid number of arguments", node);
	for (unsigned i = 0; i < node->tail.size () / 2; ++i) {
		int p = (int) type_check (node->tail.at (i * 2), SexprType::NUMBER)->value;
		SexprPtr l = type_check (node->tail.at (i * 2 + 1), SexprType::LIST);
		int len = (int) (p + l->tail.size ());
		if (len > out.size ()) out.resize (len);
		for (unsigned t = 0; t < l->tail.size (); ++t) {
			out.at (t + p) += type_check (l->tail.at (t), SexprType::NUMBER)->value;
		}
	}
	return list_from_vector (out);
}
SexprPtr gen_fn (SexprPtr node, SexprPtr env) {
	int len = (int) type_check (node->tail.at (0), SexprType::NUMBER)->value;
	std::vector<Real> coeffs;
	for (unsigned i = 1; i < node->tail.size(); ++i) {
		Real v = type_check (node->tail.at (i), SexprType::NUMBER)->value;
		coeffs.push_back (v);
	}
	std::vector<Real> table (len + 1); 
	gen10 (coeffs, table);
	return list_from_vector (table);
}
SexprPtr osc_fn (SexprPtr node, SexprPtr env) {
	Real sr = type_check (node->tail.at (0), SexprType::NUMBER)->value;
	std::vector<Real> freqs = vector_from_list (type_check (node->tail.at (1), SexprType::LIST));
	std::vector<Real> table = vector_from_list (type_check (node->tail.at (2), SexprType::LIST));
	std::vector<Real> out (freqs.size ());
	int N = table.size () - 1;
	Real fn = (Real) sr / N; // Hz
	Real phi = 0; //rand () % (N - 1);
	for (unsigned i = 0; i < freqs.size (); ++i) {
		int intphi = (int) phi;
		Real fracphi = phi - intphi;
		Real c = (1 - fracphi) * table[intphi] + fracphi * table[intphi + 1];
		out.at (i) = c;
		phi = phi + freqs.at (i) / fn;
		if (phi >= N) phi = phi - N;
	}
	return list_from_vector (out);
}
SexprPtr compute_fft (SexprPtr n, int sign) {
    std::vector<Real> d = vector_from_list (n);
 	int N = next_pow2 (d.size ());
    fft<Real> (&d[0], N / 2, sign);
  	int norm = (sign < 0 ? 1 : N / 2);
    SexprPtr res = Sexpr::make_list ();
    for (unsigned i = 0; i < N; ++i) { res->tail.push_back (Sexpr::make_number(d[i] / norm)); }
    return res;
}
SexprPtr fft_fn (SexprPtr n, SexprPtr env) {
    return compute_fft (type_check (n->tail.at (0), SexprType::LIST), -1);
}
SexprPtr ifft_fn (SexprPtr n, SexprPtr env) {
    return compute_fft (type_check (n->tail.at (0), SexprType::LIST), 1);
}
SexprPtr car2pol_fn (SexprPtr n, SexprPtr env) {
	std::vector<Real> c = vector_from_list (n->tail.at (0));
	rect2pol (&c[0], c.size () / 2);
	return list_from_vector (c);
}
SexprPtr pol2car_fn (SexprPtr n, SexprPtr env) {
 	std::vector<Real> c = vector_from_list (n->tail.at (0));
	pol2rect (&c[0], c.size () / 2);
	return list_from_vector (c);
}
SexprPtr conv_fn (SexprPtr n, SexprPtr env) {
    SexprPtr ir = type_check (n->tail.at (0), SexprType::LIST);
    SexprPtr sig = type_check (n->tail.at (1), SexprType::LIST);
    Real scale = type_check(n->tail.at (2), SexprType::NUMBER)->value;
	Real mix = 0;
	if (n->tail.size () == 4) mix = type_check(n->tail.at (3), SexprType::NUMBER)->value;
    long irsamps = ir->tail.size ();
    long sigsamps = sig->tail.size ();
    if (irsamps <= 0 || sigsamps <= 0) return Sexpr::make_list();
    int max = irsamps > sigsamps ? irsamps : sigsamps;
    int N = next_pow2(max) << 1;
    Real* fbuffir = new Real[2 * N];
    Real* fbuffsig = new Real[2 * N];
    Real* fbuffconv = new Real[2 * N];
    for (unsigned i = 0; i < N; ++i) {
        if (i < irsamps) fbuffir[2 * i] = type_check (ir->tail.at(i), SexprType::NUMBER)->value;
        else fbuffir[2 * i] = 0;
        if (i < sigsamps) fbuffsig[2 * i] = type_check (sig->tail.at(i), SexprType::NUMBER)->value;
        else fbuffsig[2 * i] = 0;
        fbuffconv[2 * i] = 0;
        fbuffir[2 * i + 1] = 0;
        fbuffsig[2 * i + 1] = 0;
        fbuffconv[2 * i + 1] = 0;        
    }
    AbstractFFT<Real>* fft = createFFT<Real>(N);
    fft->forward(fbuffir);
    fft->forward(fbuffsig);
    complexMultiplyReplace(fbuffir, fbuffsig, fbuffconv, N);
    fft->inverse(fbuffconv);
    SexprPtr wave = Sexpr::make_list();
    for (unsigned i = 0; i < (irsamps + sigsamps) -1; ++i) {
        Real s = scale * fbuffconv[2 * i] / (2 * N);
        if (i < sigsamps) s+= sig->tail.at(i)->value * mix;
        wave->tail.push_back(Sexpr::make_number(s));
    }
    delete [] fbuffir;
    delete [] fbuffsig;
    delete [] fbuffconv;
    return wave;
}
// I/O  -----------------------------------------------------------------------
SexprPtr sndwrite_fn (SexprPtr node, SexprPtr env) {
	Real sr = type_check (node->tail.at (0), SexprType::NUMBER)->value;
	std::vector<Real> vals;
	if (node->tail.size () == 3) {
		WavOutFile outf (type_check (node->tail.at (1), SexprType::STRING)->lexeme.c_str(), sr, 16, 1);
		vals = vector_from_list (type_check (node->tail.at (2), SexprType::LIST));
		outf.write (&vals[0], vals.size ());
	} else if (node->tail.size () == 4) {
		WavOutFile outf (type_check (node->tail.at (1), SexprType::STRING)->lexeme.c_str(), sr, 16, 2);
		std::vector<Real> left = vector_from_list (type_check (node->tail.at (2), SexprType::LIST));
		std::vector<Real> right = vector_from_list (type_check (node->tail.at (3), SexprType::LIST));
		vals.resize (2 * left.size ());
		interleave (&vals[0], &left[0], &right[0], left.size ());
		outf.write (&vals[0], vals.size ());
	} else error ("invalid number of channels in", node->tail.at(0));
	
	return Sexpr::make_number (vals.size ());
}
SexprPtr sndread_fn (SexprPtr node, SexprPtr env) {
	WavInFile inf (type_check (node->tail.at (0), SexprType::STRING)->lexeme.c_str());
	SexprPtr l = Sexpr::make_list ();
	int s = inf.getNumSamples ();
	std::vector<Real> input (s);
	inf.read (&input[0], s);
	SexprPtr info = Sexpr::make_list ();
	info->tail.push_back (Sexpr::make_number (inf.getSampleRate ()));
	info->tail.push_back (Sexpr::make_number (inf.getNumChannels ()));
	info->tail.push_back (Sexpr::make_number (s));
	l->tail.push_back (info);
	if (inf.getNumChannels () == 1) {
		l->tail.push_back (list_from_vector (input));
	} else if (inf.getNumChannels () == 2) {
		std::vector<Real> left (s/2);
		std::vector<Real> right (s/2);
		deinterleave (&input[0], &left[0], &right[0], s);
		l->tail.push_back (list_from_vector (left));
		l->tail.push_back (list_from_vector (right));
	} else error ("invalid number of channels in", node->tail.at (0));
	return l;
}
void add_numeric (SexprPtr env) {
	// vector operations
	add_op ("add", addv_fn, 2, env);
	add_op ("sub", subv_fn, 2, env);
	add_op ("mul", mulv_fn, 2, env);
	add_op ("div", divv_fn, 2, env);
	add_op ("min", minv_fn, 1, env);
	add_op ("max", maxv_fn, 1, env);
	add_op ("sum", sumv_fn, 1, env);
	add_op ("mean", mean_fn, 1, env);
	add_op ("sinv", sinv_fn, 1, env);
	add_op ("cosv", cosv_fn, 1, env);
	add_op ("expv", expv_fn, 1, env);
	add_op ("logv", logv_fn, 1, env);
	add_op ("sqrtv", sqrtv_fn, 1, env);
	add_op ("absv", absv_fn, 1, env);
	add_op ("floorv", floorv_fn, 1, env);
	add_op ("bpf", bpf_fn, 3, env);
	add_op ("mix", mix_fn, 2, env);	
	add_op ("gen", gen_fn, 2, env);
	add_op ("osc", osc_fn, 3, env);
	add_op ("fft", fft_fn, 1, env);
	add_op ("ifft", ifft_fn, 1, env);
	add_op ("car2pol", car2pol_fn, 1, env);
	add_op ("pol2car", pol2car_fn, 1, env);
	add_op ("conv", conv_fn, 3, env);
	// I/O
	add_op ("sndwrite", sndwrite_fn, 3, env);
	add_op ("sndread", sndread_fn, 1, env);
}
#endif	// NUMERIC_H 

// EOF
