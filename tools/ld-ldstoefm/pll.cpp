/************************************************************************

    pll.cpp

    ld-ldstoefm - LDS sample to EFM data processing
    Copyright (C) 2019 Simon Inns
    Copyright (C) 2019 Adam Sampson

    This file is part of ld-decode-tools.

    ld-ldstoefm is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

************************************************************************/

// Note: The PLL implementation is based on original code provided to
// the ld-decode project by Olivier “Sarayan” Galibert.  Many thanks
// for the assistance!

#include "pll.h"

Pll::Pll()
{
    // Default ZC detector state.
    // In order to hold state over buffer read boundaries, we keep
    // global track of the direction and delta information.
    zcPreviousInput = 0;
    delta = 0;

    // Default PLL state
    basePeriod = 40000000.0 / 4321800.0; // T1 clock period 40MSPS / bit-rate

    minimumPeriod  = basePeriod * 0.90; // -10% minimum
    maximumPeriod  = basePeriod * 1.10; // +10% maximum
    periodAdjustBase = basePeriod * 0.0001; // Clock adjustment step

    // PLL Working parameter defaults
    currentPeriod = basePeriod;
    frequencyHysteresis = 0;
    phaseAdjust = 0;
    refClockTime = 0;
    tCounter = 1;
}

// This method performs interpolated zero-crossing detection and stores the
// result a sample deltas (the number of samples between each
// zero-crossing).  Interpolation of the zero-crossing point provides a
// result with sub-sample resolution.
//
// Since the EFM data is NRZ-I (non-return to zero inverted) the polarity of the input
// signal is not important (only the frequency); therefore we can simply
// store the delta information.  The resulting delta information is fed to the
// phase-locked loop which is responsible for correcting jitter errors from the ZC
// detection process.
QByteArray Pll::process(QByteArray buffer)
{
    // Input data is really qint16 wrapped in a byte array
    const qint16 *inputBuffer = reinterpret_cast<const qint16 *>(buffer.data());

    // Clear the PLL result buffer
    pllResult.clear();

    for (qint32 i = 0; i < (buffer.size() / 2); i++) {
        qint16 vPrev = zcPreviousInput;
        qint16 vCurr = inputBuffer[i];

        // Have we seen a zero-crossing?
        if ((vPrev < 0 && vCurr >= 0) || (vPrev >= 0 && vCurr < 0)) {
            // Interpolate to get the ZC sub-sample position fraction
            qreal prev = static_cast<qreal>(vPrev);
            qreal curr = static_cast<qreal>(vCurr);
            qreal fraction = (-prev) / (curr - prev);

            // Feed the sub-sample accurate result to the PLL
            pushEdge(delta + fraction);

            // Offset the next delta by the fractional part of the result
            // in order to maintain accuracy
            delta = 1.0 - fraction;
        } else {
            // No ZC, increase delta by 1 sample
            delta += 1.0;
        }

        // Keep the previous input (so we can work across buffer boundaries)
        zcPreviousInput = inputBuffer[i];
    }

    return pllResult;
}

// Called when a ZC happens on a sample number
void Pll::pushEdge(qreal sampleDelta)
{
    while (sampleDelta >= refClockTime) {
        qreal next = refClockTime + currentPeriod + phaseAdjust;
        refClockTime = next;

        // Note: the tCounter < 3 check causes an 'edge push' if T is 1 or 2 (which
        // are invalid timing lengths for the NRZI data).  We also 'edge pull' values
        // greater than T11
        if ((sampleDelta > next || tCounter < 3) && tCounter < 11) {
            phaseAdjust = 0;
            tCounter++;
        } else {
            qreal edgeDelta = sampleDelta - (next - currentPeriod / 2.0);
            phaseAdjust = edgeDelta * 0.005;

            // Adjust frequency based on error
            if (edgeDelta < 0) {
                if (frequencyHysteresis < 0)
                    frequencyHysteresis--;
                else
                    frequencyHysteresis = -1;
            } else if (edgeDelta > 0) {
                if (frequencyHysteresis > 0)
                    frequencyHysteresis++;
                else
                    frequencyHysteresis = 1;
            } else {
                frequencyHysteresis = 0;
            }

            // Update the reference clock?
            if (frequencyHysteresis < -1.0 || frequencyHysteresis > 1.0) {
                qreal aper = periodAdjustBase * edgeDelta / currentPeriod;

                // If there's been a substantial gap since the last edge (e.g.
                // a dropout), edgeDelta can be very large here, so we need to
                // limit how much of an adjustment we're willing to make
                if (aper < -periodAdjustBase)
                    aper = -periodAdjustBase;
                else if (aper > periodAdjustBase)
                    aper = periodAdjustBase;

                currentPeriod += aper;

                if (currentPeriod < minimumPeriod) {
                    currentPeriod = minimumPeriod;
                } else if (currentPeriod > maximumPeriod) {
                    currentPeriod = maximumPeriod;
                }
            }

            pllResult.push_back(tCounter);
            tCounter = 1;
        }
    }

    // Reset refClockTime ready for the next delta but
    // keep any error to maintain accuracy
    refClockTime -= sampleDelta;

    // Use this debug if you want to monitor the PLL output frequency
    //qDebug() << "Base =" << basePeriod << "current = " << currentPeriod;
}
