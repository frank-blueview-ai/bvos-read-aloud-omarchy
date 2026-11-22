"""Keep the local Kokoro model loaded and return mono PCM blocks to the app."""

import json
import os
import re
from pathlib import Path
import sys
from base64 import b64encode

from kokoro_onnx import Kokoro
import numpy as np
import onnxruntime as ort
import langid


VOICES = {
    "en": ("af_heart", "en-us"),
    "en-gb": ("bf_emma", "en-gb"),
    "es": ("ef_dora", "es"),
    "fr": ("ff_siwis", "fr-fr"),
    "it": ("if_sara", "it"),
    "pt": ("pf_dora", "pt-br"),
    "hi": ("hf_alpha", "hi"),
    "ja": ("jf_alpha", "ja"),
    "zh": ("zf_xiaobei", "cmn"),
}


LANGUAGE_WORDS = {
    "english": "en", "spanish": "es", "español": "es",
    "french": "fr", "français": "fr", "italian": "it", "italiano": "it",
    "portuguese": "pt", "português": "pt", "hindi": "hi",
    "japanese": "ja", "mandarin": "zh", "chinese": "zh",
}


def choose_language(text, hint):
    # Instructions such as "answer in French" override the language of the question.
    requested = re.search(
        r"\b(?:reply|respond|answer|write|explain|translate|say)\b[^.!?\n]{0,80}?"
        r"\b(?:in|into|to)\s+(" + "|".join(LANGUAGE_WORDS) + r")\b",
        hint, re.IGNORECASE,
    )
    if requested:
        return LANGUAGE_WORDS[requested.group(1).lower()]

    def words(value):
        value = re.sub(r"\$\$.*?\$\$|\$[^$\n]*\$|\\[A-Za-z]+", " ", value, flags=re.DOTALL)
        return "".join(ch if ch.isalpha() else " " for ch in value)

    sample, original = words(text), words(hint)
    # A tiny opening often contains only proper nouns or mathematical notation.
    # Use the original request as additional evidence until more prose is available.
    if len(sample.split()) < 18 and len(original.split()) >= 3:
        sample = original + " " + original + " " + sample
    elif len(sample.split()) < 3 and original.strip():
        sample = original
    return langid.classify(sample)[0] if sample.strip() else "en"


def emit(message):
    print(json.dumps(message, separators=(",", ":")), flush=True)


def main():
    directory = Path(__file__).resolve().parent
    ort.disable_telemetry_events()
    options = ort.SessionOptions()
    options.intra_op_num_threads = min(6, os.cpu_count() or 1)
    session = ort.InferenceSession(
        str(directory / "kokoro-v1.0.onnx"),
        sess_options=options,
        providers=["CPUExecutionProvider"],
    )
    engine = Kokoro.from_session(session, str(directory / "voices-v1.0.bin"))
    generation = None
    voice = None
    for line in sys.stdin:
        request = json.loads(line)
        try:
            if request["id"] != generation:
                generation = request["id"]
                language = request.get("language", "en")
                if language == "auto":
                    language = choose_language(request["text"], request.get("hint", ""))
                voice = VOICES.get(language)
            if voice is None:
                emit({"id": generation, "error": "unsupported_language"})
                continue
            name, locale = voice
            samples, rate = engine.create(request["text"], voice=name, speed=1.0, lang=locale)
            if rate != 24000 or not samples.size:
                raise ValueError("Unexpected speech audio format")
            pcm = (np.clip(samples, -1, 1) * 32767).astype("<i2")
            emit({"id": request["id"], "language": language, "audio": b64encode(pcm.tobytes()).decode("ascii")})
        except Exception:
            emit({"id": request["id"], "error": "synthesis"})


if __name__ == "__main__":
    main()
