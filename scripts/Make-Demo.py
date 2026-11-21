"""Create a reproducible watery/compressed test from a held-out synthetic phrase.
The cleaner reference is a different utterance; no user recording is included.
"""
from pathlib import Path
import argparse, json, wave
import numpy as np

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--output',default=str(root/'build/demo'));args=parser.parse_args()
out=Path(args.output);out.mkdir(parents=True,exist_ok=True)
def read(path):
    with wave.open(str(path)) as f:
        assert f.getsampwidth()==2 and f.getnchannels()==1
        return np.frombuffer(f.readframes(f.getnframes()),dtype='<i2').astype(np.float64)/32768,f.getframerate()
def write(path,x,rate=48000):
    with wave.open(str(path),'wb') as f:
        f.setparams((1,2,rate,0,'NONE','not compressed'));f.writeframes(np.rint(np.clip(x,-.999,.999)*32767).astype('<i2').tobytes())
clean,rate=read(root/'resources/tests/clean-evaluation.wav')
clean=np.interp(np.arange(round(len(clean)*48000/rate))*rate/48000,np.arange(len(clean)),clean)
clean*=.72/max(.72,np.max(np.abs(clean)))
size=2048;hop=512;window=np.hanning(size+1)[:-1];rng=np.random.default_rng(9127)
padded=np.pad(clean,(size,size));render=np.zeros_like(padded);normalizer=np.zeros_like(padded)
hz=np.fft.rfftfreq(size,1/48000)
color=(1-.62*np.exp(-.5*(np.log(np.maximum(hz,40)/1250)/.63)**2))*(.38+.62/(1+(hz/3400)**6))
mask=np.ones(len(hz));drop=np.ones(len(hz));counter=0
for start in range(0,len(padded)-size+1,hop):
    spectrum=np.fft.rfft(padded[start:start+size]*window)
    if counter%4==0:
        drop=np.where(rng.random(len(hz))<.36,.035,1.0)
        drop[:3]=1
    mask=.55*mask+.45*drop
    # Spectral subtraction-like holes plus an uneven/muffled spectral balance.
    processed=np.fft.irfft(spectrum*color*mask,size)
    render[start:start+size]+=processed*window;normalizer[start:start+size]+=window*window
    counter+=1
damaged=(render/np.maximum(normalizer,1e-12))[size:size+len(clean)]
# A slow compressor reduces the original loudness contour without clipping.
envelope=np.sqrt(np.convolve(damaged*damaged,np.ones(1200)/1200,mode='same')+1e-8)
damaged*=np.clip((.045/np.maximum(envelope,.006))**.30,.5,2.5)
active=np.abs(clean)>.006
damaged*=np.sqrt(np.mean(clean[active]**2)/max(1e-12,np.mean(damaged[active]**2)))
write(out/'01-clean-evaluation.wav',clean);write(out/'02-watery-damaged.wav',damaged)
(out/'fixture.json').write_text(json.dumps({'synthetic_speech':True,'reference_is_different_utterance':True,'rate':48000,'samples':len(clean),'damage':'time-varying spectral holes, spectral color and compression'},indent=2))
print(json.dumps({'clean':str(out/'01-clean-evaluation.wav'),'damaged':str(out/'02-watery-damaged.wav'),'samples':len(clean)}))
