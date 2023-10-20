// BlockVocoder.h
// 

#ifndef BLOCKVOCODER_H
#define BLOCKVOCODER_H 

//#define NO_PEAKS_ESTIMATION
//#define USE_SLOW_FFT

#include "FFT.h"
#include <iostream>
#include <complex>
#include <fstream>

namespaece nanoSound {
	static const double EPS = .00001;

	template <typename T>
//! Advanced phase-vocoder with formants preservation and phase-sync processing.
/*!
  The input of the process function is a workspace ready for FFT; the input is wksp while
  the output is wksp2; note the the input is also transformed during processing.
  The data in wksp MUST be windowed correctly and the output wksp2 must be overlapp-added.
*/
		class BlockVocoder {
	private:
		BlockVocoder& operator= (BlockVocoder&);
		BlockVocoder (const BlockVocoder&);
	public: 
		BlockVocoder (int fftSize) {
			m_N  = fftSize;
			m_NN = m_N << 1;
			m_N2 = m_N >> 1;		
			m_sicvt = TWOPI / (double) m_N;

			// FFT
			m_transform = createFFT<T> (m_N);

			// working buffers
			m_peaks = new int[m_N];
			memset (m_peaks, 0, sizeof (int) * m_N);

			// phases
			m_psi = new T[m_N];
			memset (m_psi, 0, sizeof (T) * m_N);
			m_phaseInc = new T[m_N];
			memset (m_phaseInc, 0, sizeof (T) * m_N);
			m_oldAnPhi = new T[m_N];
			memset (m_oldAnPhi, 0, sizeof (T) * m_N);
			m_oldSynPhi = new T[m_N];
			memset (m_oldSynPhi, 0, sizeof (T) * m_N);
		}
		virtual ~BlockVocoder () {
			delete [] m_peaks;

			delete [] m_psi;
			delete [] m_phaseInc;
			delete [] m_oldAnPhi;
			delete [] m_oldSynPhi;

			delete m_transform;
		}

		void process (T* wksp, T* wksp2, T P, T D, T I, int C, T threshold) {
			fftshift<T> (wksp, m_NN);	
#ifdef USE_SLOW_FFT	
			fft<T> (wksp, m_N, -1);	
#else
			m_transform->forward (wksp);
#endif
			rect2pol<T> (wksp, m_N);

#ifdef NO_PEAKS_ESTIMATION
			double stretch = I / D;
			double omega = 0, phase = 0, delta = 0;
			for (int i = 0; i < m_N; ++i) {
				omega = m_sicvt * (double) i * D;
				phase = wksp[2 * i + 1];
				delta = omega + princarg (phase - m_oldAnPhi[i] - omega);
				m_oldAnPhi[i] = phase;
				m_psi[i] = princarg (m_psi[i] + delta * stretch * P);
				wksp[2 * i + 1] = m_psi[i];
			}
#else
			// phase increment computation
			T omega = 0, delta = 0;
			for (int i = 0; i < m_N2; ++i) {
				// T f = parabolic<T> (wksp, i, m_N);
				omega = m_sicvt * (double) i * D;
				delta = princarg<T> (wksp[2 * i + 1] - m_oldAnPhi[i] - omega);
				m_phaseInc[i] = P * (omega + delta) / D;
			}

			// peak picking
			int peaksNum = locmax2 (wksp, m_N2, m_peaks);
			if (peaksNum == 0) ++peaksNum;

			memset (m_psi, 0, sizeof (T) * m_N);
			for (int i = 0; i < peaksNum; ++i) {
				m_psi[m_peaks[i]] = m_oldSynPhi[m_peaks[i]] + (T) I * m_phaseInc[m_peaks[i]];		
			}

			// phase locking
			int start = 0;
			for (int i = 0; i < peaksNum - 1; ++i) {
				int pk = m_peaks[i];
				int next_pk = m_peaks[i + 1];
				int end = (int) round ((T)(pk + next_pk) * .5);
				T angRotation = m_psi[m_peaks[i]] - wksp[2 * m_peaks[i] + 1];
				for (int j = start; j <= end; ++j) {
					if (j != pk) {
						m_psi[j] = angRotation + wksp[2 * j + 1];
					}
				}
				start = end + 1;
			}

			// backup current phases for next round
			for (int i =  0; i < m_N2; ++i) {
				m_oldAnPhi[i] = wksp[2 * i + 1];
				m_oldSynPhi[i] = m_psi[i];
			}
#endif
			if (C) {
				// compute inverse transposition in wksp2
				resample (wksp, wksp2, 1. / P);
				// outputs: wksp has the corrected spectrum, wksp2 has the envelope
				cepstralEnvelopeCorrection (C, wksp, wksp2);
			} 

			resample (wksp, wksp2, P, threshold);

			// synthesis
			pol2rect<T> (wksp2, m_N);
#ifdef USE_SLOW_FFT
			fft<T> (wksp2, m_N, 1);
#else
			m_transform->inverse (wksp2);
#endif
			fftshift<T> (wksp2, m_NN); 
		}

		void reset () {
			memset (m_oldSynPhi, 0, sizeof (T) * m_N);
			memset (m_oldAnPhi, 0, sizeof (T) * m_N);
		}
	private:
		void resample (const T* wksp, T* wksp2, T ratio, T threshold = 0) {
			// frequency domain resampling
			memset (wksp2, 0, sizeof (T) * m_NN);
			int k = 1;
			int pos = (int) ((T) k * ratio);
			while (pos < m_N2 && k < m_N * ratio) {
				wksp2[2 * pos] = wksp[2 * k] <= threshold ? 0 : wksp[2 * k]; // denoise
				wksp2[2 * pos + 1] = m_psi[k];
				++k;
				pos = (int) ((T) k * ratio);
			}
			for (int i = 0; i < m_N2; i++) {
				int pos = (2 * (m_N - i)) - 2;
				wksp2[pos] = wksp2[2 * i];
				wksp2[pos + 1] = -wksp2[2 * i + 1];
			}
		}
		void cepstralEnvelopeCorrection (int C, T* wksp, T* wksp2) {
			// compute cepstrum on the correction
			for (int i = 0; i < m_N; ++i) {
				// NB: the division with m_NN is only for NUMERICAL PROBLEMS since the result
				// should be the same with or without the division (log (a / n) - log (b / n) == log (a) 
				wksp2[2 * i]  = log ((T) (wksp2[2 * i] / m_NN) + EPS) -  log ((T) (wksp[2 * i] / m_NN) + EPS);	
				// wksp2[2 * i] = log (EPS + wksp2[2 * i] / (wksp[2 * i] + EPS));
				wksp2[2 * i + 1] = 0.;
			}

			// inverse transform
#ifdef USE_SLOW_FFT
			fft<T> (wksp2, m_N, 1);
#else
			m_transform->inverse (wksp2);
#endif		

			for (int i = 0; i < m_N; ++i) {
				wksp2[2 * i] /= m_N;
				wksp2[2 * i + 1] = 0;
			}
	
			// liftering
			wksp2[0] *= .5;
			for (int i = C + 1; i < m_N; ++i) {
				wksp2[2 * i] = 0;
			}

			// spectral envelope
#ifdef USE_SLOW_FFT
			fft<T> (wksp2, m_N, -1);		
#else
			m_transform->forward (wksp2);
#endif

			// apply correction
			for (int i = 0; i < m_N; ++i) {
				wksp[2 * i] *= exp (2. * wksp2[2 * i]);
			}
		}

		int* m_peaks;

		T* m_psi;
		T* m_phaseInc;
		T* m_oldAnPhi;
		T* m_oldSynPhi;

		AbstractFFT<T>* m_transform;

		int m_N;
		int m_NN;
		int m_N2;
		T m_sicvt;
	};
}
#endif	// BLOCKVOCODER_H 

// EOF
