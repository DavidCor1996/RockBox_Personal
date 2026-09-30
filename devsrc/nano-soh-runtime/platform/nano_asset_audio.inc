/* Compressed sample bytes stay on storage. Only metadata, instruments and
 * sequence programs become native pointers owned by the resource session. */
static void *sample(struct decode *d)
{
    SoundFontSample *s = reserve(d, 1, sizeof(*s));
    unsigned codec = integer(d, 1), medium = integer(d, 1);
    unsigned bit26 = integer(d, 1), relocated = integer(d, 1);
    uint32_t bytes = count(d, NANO_SAMPLE_STRIDE - 4096);
    if ((codec != CODEC_ADPCM && codec != CODEC_SMALL_ADPCM) ||
        medium != MEDIUM_RAM || bit26 > 1 || relocated > 1)
        d->error = NANO_ASSET_TYPE;
    read_bytes(d, NULL, bytes);
    AdpcmLoop *loop = reserve(d, 1, sizeof(*loop));
    uint32_t start = integer(d, 4), end = integer(d, 4),
             repeats = integer(d, 4);
    uint32_t states = count(d, 16);
    read_bytes(d, loop ? loop->predictorState : NULL, states * 2);
    AdpcmBook *book = reserve(d, 1, sizeof(*book));
    uint32_t order = count(d, 2), predictors = count(d, 8), nc = count(d, 128);
    if (nc != 8 * order * predictors || start > end)
        d->error = NANO_ASSET_FORMAT;
    int16_t *coefficients = reserve(d, nc, sizeof(*coefficients));
    read_bytes(d, coefficients, nc * sizeof(*coefficients));
    size_t index = d->asset - d->assets->slots;
    if (index >= NANO_SAMPLE_SLOTS)
        d->error = NANO_ASSET_BUDGET;
    if (s && !d->error) {
        s->codec = codec;
        s->medium = MEDIUM_CART;
        s->unk_bit26 =
            0; /* Use bounded DMA blocks, not whole-sample preload. */
        s->isRelocated = 1;
        s->size = bytes;
        s->fileSize = bytes;
        s->sampleAddr = (void *)(NANO_SAMPLE_BASE + index * NANO_SAMPLE_STRIDE);
        s->loop = loop;
        s->book = book;
        loop->start = start;
        loop->loopEnd = end;
        loop->count = repeats;
        book->order = order;
        book->npredictors = predictors;
        book->book = coefficients;
    }
    d->asset->count = bytes;
    d->asset->format = codec;
    return s;
}

static void *sequence(struct decode *d)
{
    SequenceData *s = reserve(d, 1, sizeof(*s));
    uint32_t n = count(d, 65535);
    char *data = reserve(d, n, 1);
    read_bytes(d, data, n);
    unsigned id = integer(d, 1), medium = integer(d, 1), policy = integer(d, 1);
    uint32_t fonts = count(d, 16);
    read_bytes(d, s ? s->fonts : NULL, fonts);
    if (s) {
        s->seqData = data;
        s->seqDataSize = n;
        s->seqNumber = id;
        s->medium = medium;
        s->cachePolicy = policy;
        s->numFonts = fonts;
    }
    d->asset->count = n;
    return s;
}

static AdsrEnvelope *envelope(struct decode *d)
{
    uint32_t n = count(d, 1024);
    AdsrEnvelope *env = reserve(d, n, sizeof(*env));
    for (uint32_t i = 0; i < n; ++i) {
        uint16_t delay = integer(d, 2), arg = integer(d, 2);
        /* Audio_AdsrUpdate consumes the original big-endian envelope fields. */
        if (env) {
            env[i].delay = (delay >> 8) | (delay << 8);
            env[i].arg = (arg >> 8) | (arg << 8);
        }
    }
    return env;
}

static SoundFontSound sound(struct decode *d, int present)
{
    SoundFontSound result = {0};
    if (present) {
        integer(d, 1); /* Serialized has-sample marker, followed by path. */
        char *name = string(d, 0);
        result.tuningAsU32 = integer(d, 4);
        result.sample = dependency(d, name, TYPE_SAMPLE);
    }
    return result;
}

static void *soundfont(struct decode *d)
{
    SoundFont *font = reserve(d, 1, sizeof(*font));
    uint32_t id = integer(d, 4);
    integer(d, 1);
    integer(d, 1); /* Medium and cache policy live in audio table. */
    uint16_t banks = integer(d, 2);
    integer(d, 2);
    integer(d, 2);
    uint32_t nd = count(d, 255), ni = count(d, 255), ns = count(d, 65535);
    Drum **drums = reserve(d, nd, sizeof(*drums));
    Instrument **instruments = reserve(d, ni, sizeof(*instruments));
    SoundFontSound *effects = reserve(d, ns, sizeof(*effects));
    for (uint32_t i = 0; i < nd && !d->error; ++i) {
        Drum *drum = reserve(d, 1, sizeof(*drum));
        uint8_t release = integer(d, 1), pan = integer(d, 1);
        integer(d, 1);
        AdsrEnvelope *env = envelope(d);
        SoundFontSound s = sound(d, 1);
        if (drum) {
            drum->releaseRate = release;
            drum->pan = pan;
            drum->envelope = env;
            drum->sound = s;
            drums[i] = s.sample ? drum : NULL;
        }
    }
    for (uint32_t i = 0; i < ni && !d->error; ++i) {
        Instrument *inst = reserve(d, 1, sizeof(*inst));
        uint8_t valid = integer(d, 1);
        integer(d, 1);
        uint8_t low = integer(d, 1), high = integer(d, 1),
                release = integer(d, 1);
        AdsrEnvelope *env = envelope(d);
        SoundFontSound low_note = sound(d, integer(d, 1));
        SoundFontSound normal_note = sound(d, integer(d, 1));
        SoundFontSound high_note = sound(d, integer(d, 1));
        if (inst) {
            inst->normalRangeLo = low;
            inst->normalRangeHi = high;
            inst->releaseRate = release;
            inst->envelope = env;
            inst->lowNotesSound = low_note;
            inst->normalNotesSound = normal_note;
            inst->highNotesSound = high_note;
            instruments[i] = valid ? inst : NULL;
        }
    }
    for (uint32_t i = 0; i < ns && !d->error; ++i) {
        SoundFontSound s = sound(d, integer(d, 1));
        if (effects)
            effects[i] = s;
    }
    if (font) {
        font->numInstruments = ni;
        font->numDrums = nd;
        font->numSfx = ns;
        font->sampleBankId1 = banks >> 8;
        font->sampleBankId2 = banks;
        font->instruments = instruments;
        font->drums = drums;
        font->soundEffects = effects;
        font->fntIndex = id;
    }
    return font;
}
