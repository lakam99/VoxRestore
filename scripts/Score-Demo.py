"""Measure the controlled demonstration, not perceptual certification."""
from pathlib import Path
import argparse,json,wave
import numpy as np
parser=argparse.ArgumentParser();parser.add_argument('folder',type=Path);args=parser.parse_args();folder=args.folder
def read(name):
    with wave.open(str(folder/name)) as f:
        width=f.getsampwidth();frames=f.getnframes();rate=f.getframerate();raw=f.readframes(frames)
        if width==2:x=np.frombuffer(raw,dtype='<i2').astype(np.float64)/32768
        elif width==3:
            b=np.frombuffer(raw,dtype=np.uint8).reshape(-1,3).astype(np.int32);x=(b[:,0]|b[:,1]<<8|b[:,2]<<16);x=((x^0x800000)-0x800000)/8388608
        else:raise ValueError(width)
        return x,rate
clean,rate=read('01-clean-evaluation.wav');damaged,_=read('02-watery-damaged.wav');restored,_=read('03-restored.wav')
assert len(clean)==len(damaged)==len(restored)
n=2048;hop=512;w=np.hanning(n+1)[:-1];hz=np.fft.rfftfreq(n,1/rate);band=(hz>100)&(hz<8000)
def compare(x):
    numerator=denominator=0.;distance=[]
    for start in range(0,len(clean)-n,hop):
        reference=clean[start:start+n];value=x[start:start+n]
        if np.sqrt(np.mean(reference**2))<.012:continue
        a=np.abs(np.fft.rfft(reference*w));b=np.abs(np.fft.rfft(value*w))
        a/=max(1e-12,np.linalg.norm(a));b/=max(1e-12,np.linalg.norm(b))
        # Averages over 24 broad log bands reduce dependence on phase/zero bins.
        e=np.geomspace(100,8000,25);db=[]
        for lo,hi in zip(e[:-1],e[1:]):
            indices=(hz>=lo)&(hz<hi)
            if indices.any():db.append(10*np.log10((np.mean(b[indices]**2)+1e-10)/(np.mean(a[indices]**2)+1e-10)))
        distance.append(np.mean(np.abs(db)));numerator+=np.dot(reference,reference);denominator+=np.dot(reference-value,reference-value)
    return {'mean_voice_band_error_db':float(np.mean(distance)),'snr_to_clean_db':float(10*np.log10(numerator/max(1e-12,denominator))),'peak':float(np.max(np.abs(x)))}
before=compare(damaged);after=compare(restored)
report={'synthetic_demonstration_only':True,'samples_preserved':True,'reference_is_different_utterance':True,'damaged':before,'restored':after,'spectral_error_improvement_db':before['mean_voice_band_error_db']-after['mean_voice_band_error_db'],'snr_improvement_db':after['snr_to_clean_db']-before['snr_to_clean_db']}
(folder/'comparison.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
