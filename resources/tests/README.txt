clean-reference.wav and clean-evaluation.wav are locally synthesized test speech
from Windows' speech synthesizer. They are different utterances of the same
synthetic voice. They contain no user's film recording or private dialogue.
Make-Demo.py damages the held-out evaluation phrase with deterministic spectral
holes, uneven filtering and compression. The reference is not that same phrase.
Score-Demo.py measures controlled spectral error and SNR; these are not guarantees
of perceived quality on real heavily damaged human speech.
