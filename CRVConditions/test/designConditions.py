#!/usr/bin/env python3
# Nominal CRV conditions for a CRV geometry without hardware (e.g. a design study):
#   <name>.txt              channel map, offline channels numbered sequentially into FEBs (64 channels) and ROCs (24 FEBs)
#   photonyield_<name>.txt  CRVPhoton, gaussian photon yield deviations
#   status_<name>.txt       CRVBadChan, all channels good
#   calib_<name>.txt        CRVTime (gaussian time offset per FEB) and CRVSiPM (nominal pedestal, pulse height, pulse area)
# <name> is crs.name of the geometry; nChannels is the number of bars times 4.
# The map is found by CRVOrdinal with filePath set to the output directory (relative to MU2E_SEARCH_PATH);
# the tables go into DbService.textFile.
import argparse
import os
import random

parser = argparse.ArgumentParser(description="nominal CRV conditions for a CRV geometry without hardware")
parser.add_argument("nChannels", type=int, help="number of offline channels (number of bars times 4)")
parser.add_argument("name", help="crs.name of the geometry")
parser.add_argument("outdir", help="output directory")
parser.add_argument("--photonYieldSigma", type=float, default=0.05, help="sigma of the photon yield deviations (Run 2 nominal: 0.05)")
parser.add_argument("--timeOffsetSigma", type=float, default=1.0, help="sigma of the FEB time offsets in ns (Run 2 nominal: 1.0)")
parser.add_argument("--pedestal", type=float, default=100.0)
parser.add_argument("--pulseHeight", type=float, default=11.4)
parser.add_argument("--pulseArea", type=float, default=394.6)
parser.add_argument("--seed", type=int, default=1)
args = parser.parse_args()

nChanPerFEB = 64
nFEBPerROC = 24
nChannels = args.nChannels
random.seed(args.seed)
os.makedirs(args.outdir, exist_ok=True)

def truncatedGauss(sigma, limitlow, limithigh):
  while True:
    r = random.gauss(0, sigma)
    if r > limitlow and r < limithigh:
      return r

# channel map: ROC and FEB numbers start at 1, FEB channels at 0
with open(os.path.join(args.outdir, args.name + ".txt"), "w") as f:
  f.write("Channel\tROC\tFEB\tFEBchannel\n")
  for channel in range(nChannels):
    globalFEB = channel // nChanPerFEB
    f.write("{}\t{}\t{}\t{}\n".format(channel, globalFEB // nFEBPerROC + 1, globalFEB % nFEBPerROC + 1, channel % nChanPerFEB))

with open(os.path.join(args.outdir, "photonyield_" + args.name + ".txt"), "w") as f:
  f.write("TABLE CRVPhoton\n")
  f.write("#{} channels, sigma: {}, limitlow: -0.2, limithigh: 0.2\n".format(nChannels, args.photonYieldSigma))
  f.write("#channel,photonYieldDeviation\n")
  for channel in range(nChannels):
    f.write("{},{:.3f}\n".format(channel, truncatedGauss(args.photonYieldSigma, -0.2, 0.2) if args.photonYieldSigma > 0 else 0.0))

with open(os.path.join(args.outdir, "status_" + args.name + ".txt"), "w") as f:
  f.write("TABLE CRVBadChan\n")
  f.write("#{} channels, all good\n".format(nChannels))
  f.write("#channel,status\n")

with open(os.path.join(args.outdir, "calib_" + args.name + ".txt"), "w") as f:
  f.write("TABLE CRVTime\n")
  f.write("#{} channels, sigma: {}, limitlow: -3.0, limithigh: 3.0\n".format(nChannels, args.timeOffsetSigma))
  f.write("#channel,timeOffset\n")
  timeOffsetsFEB = {}
  for channel in range(nChannels):
    globalFEB = channel // nChanPerFEB
    if globalFEB not in timeOffsetsFEB:
      timeOffsetsFEB[globalFEB] = truncatedGauss(args.timeOffsetSigma, -3.0, 3.0) if args.timeOffsetSigma > 0 else 0.0
    f.write("{},{:.3f}\n".format(channel, timeOffsetsFEB[globalFEB]))
  f.write("TABLE CRVSiPM\n")
  f.write("#channel,pedestal,calibPulseHeight,calibPulseArea\n")
  for channel in range(nChannels):
    f.write("{},{},{},{}\n".format(channel, args.pedestal, args.pulseHeight, args.pulseArea))

print("wrote conditions for {} channels ({} FEBs, {} ROCs) of {} to {}".format(
    nChannels, (nChannels + nChanPerFEB - 1) // nChanPerFEB,
    (nChannels + nChanPerFEB * nFEBPerROC - 1) // (nChanPerFEB * nFEBPerROC), args.name, args.outdir))
