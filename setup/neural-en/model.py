"""
GRU encoder-decoder architecture for the neural G2P model -- MUST match
src/TinyTTSTools/G2P/G2PNeuralModel.h's gruStep()/predict() exactly:
single-layer nn.GRU for encoder and decoder (PyTorch's GRU gate order is
[reset, update, new], the same order gruStep() assumes), decoder hidden
state initialized directly from the encoder's final hidden state (no
bridge/projection layer -- the reference implementation this was ported
from does the same), and a plain Linear output projection.
"""
import torch
import torch.nn as nn

from vocab import NUM_GRAPHEMES, NUM_PHONEMES, BOS


class G2PModel(nn.Module):
    def __init__(self, hidden_dim: int = 256, dropout: float = 0.0):
        super().__init__()
        self.hidden_dim = hidden_dim
        self.enc_emb = nn.Embedding(NUM_GRAPHEMES, hidden_dim)
        self.dec_emb = nn.Embedding(NUM_PHONEMES, hidden_dim)
        # batch_first=False (default) -- sequences as [seq_len, batch, hidden]
        self.encoder = nn.GRU(hidden_dim, hidden_dim, num_layers=1)
        self.decoder = nn.GRU(hidden_dim, hidden_dim, num_layers=1)
        self.fc = nn.Linear(hidden_dim, NUM_PHONEMES)
        # Embedding dropout only -- adds no parameters and is a no-op in
        # eval() (nn.Dropout is identity when self.training is False), so
        # it changes nothing about the exported weights or the inference
        # architecture gruStep()/predict() in G2PNeuralModel.h assume;
        # purely a training-time regularizer for the CMUdict-scale English
        # task, where overfitting (train loss still falling while val
        # exact-match plateaus) was observed without it.
        self.emb_dropout = nn.Dropout(dropout)

    def encode(self, enc_input, enc_lengths):
        """enc_input: [seq_len, batch] grapheme indices (already including
        the trailing </s>=2 marker). Returns final hidden [1, batch, hidden]."""
        embedded = self.emb_dropout(self.enc_emb(enc_input))
        packed = nn.utils.rnn.pack_padded_sequence(
            embedded, enc_lengths.cpu(), enforce_sorted=False)
        _, h = self.encoder(packed)
        return h

    def forward(self, enc_input, enc_lengths, dec_input):
        """Teacher-forced training forward pass.
        dec_input: [seq_len, batch] decoder input indices (BOS + target[:-1]).
        Returns logits [seq_len, batch, NUM_PHONEMES]."""
        h = self.encode(enc_input, enc_lengths)
        dec_embedded = self.emb_dropout(self.dec_emb(dec_input))
        out, _ = self.decoder(dec_embedded, h)
        return self.fc(out)

    @torch.no_grad()
    def greedy_decode(self, enc_input, enc_lengths, max_len: int = 24):
        """Batched greedy decode (no teacher forcing). Returns
        [max_len, batch] predicted class indices (run for the full
        max_len; caller truncates each sequence at its own first EOS)."""
        h = self.encode(enc_input, enc_lengths)
        batch = enc_input.size(1)
        device = enc_input.device
        cur = torch.full((1, batch), BOS, dtype=torch.long, device=device)
        outputs = []
        for _ in range(max_len):
            embedded = self.dec_emb(cur)
            out, h = self.decoder(embedded, h)
            logits = self.fc(out)  # [1, batch, NUM_PHONEMES]
            cur = logits.argmax(dim=-1)  # [1, batch]
            outputs.append(cur)
        return torch.cat(outputs, dim=0)  # [max_len, batch]
