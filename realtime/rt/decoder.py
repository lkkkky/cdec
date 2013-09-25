import logging
import os
import subprocess
import threading

import util

class Decoder:

    def close(self, force=False):
        if not force:
            self.semaphore.acquire()
        self.decoder.stdin.close()
        if not force:
            self.semaphore.release()

    def decode(self, sentence, grammar=None):
        '''Threadsafe'''
        input = '<seg grammar="{g}">{s}</seg>\n'.format(s=sentence, g=grammar) if grammar else '{}\n'.format(sentence)
        self.semaphore.acquire()
        self.decoder.stdin.write(input)
        hyp = self.decoder.stdout.readline().strip()
        self.semaphore.release()
        return hyp

class CdecDecoder(Decoder):

    def __init__(self, config, weights):
        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        decoder = os.path.join(cdec_root, 'decoder', 'cdec')
        decoder_cmd = [decoder, '-c', config, '-w', weights]
        logging.info('Executing: {}'.format(' '.join(decoder_cmd)))
        self.decoder = util.popen_io(decoder_cmd)
        self.semaphore = threading.Semaphore()

class MIRADecoder(Decoder):

    def __init__(self, config, weights):
        cdec_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        mira = os.path.join(cdec_root, 'training', 'mira', 'kbest_cut_mira')
        #                                              optimizer=2 step=0.001    best=500,    k=500,       uniq, stream
        mira_cmd = [mira, '-c', config, '-w', weights, '-o', '2', '-C', '0.001', '-b', '500', '-k', '500', '-u', '-t']
        logging.info('Executing: {}'.format(' '.join(mira_cmd)))
        self.decoder = util.popen_io(mira_cmd)
        self.semaphore = threading.Semaphore()

    def get_weights(self):
        '''Threadsafe'''
        self.semaphore.acquire()
        self.decoder.stdin.write('WEIGHTS ||| WRITE\n')
        weights = self.decoder.stdout.readline().strip()
        self.semaphore.release()
        return weights

    def set_weights(self, w_line):
        '''Threadsafe'''
        self.semaphore.acquire()
        self.decoder.stdin.write('WEIGHTS ||| {}\n'.format(w_line))
        self.semaphore.release()

    def update(self, sentence, grammar, reference):
        '''Threadsafe'''
        input = 'LEARN ||| <seg grammar="{g}">{s}</seg> ||| {r}\n'.format(s=sentence, g=grammar, r=reference)
        self.semaphore.acquire()
        self.decoder.stdin.write(input)
        log = self.decoder.stdout.readline().strip()
        self.semaphore.release()
        return log
