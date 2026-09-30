/* Included by nano_assets.c to share its bounded, two-pass reader. */
static void *mesh(struct decode *d)
{
    integer(d, 1);
    uint8_t type = integer(d, 1);
    uint32_t n = type == 1 ? 1 : integer(d, 1);
    MeshHeader *h = reserve(d, 1, sizeof(*h));
    if (h)
        h->base.type = type;
    if (type == 0 || type == 2) {
        size_t size = type == 0 ? sizeof(PolygonDlist) : sizeof(PolygonDlist2);
        void *entries = reserve(d, n, size);
        for (uint32_t i = 0; i < n; ++i) {
            integer(d, 1);
            int16_t pos[4] = {0};
            if (type == 2)
                read_bytes(d, pos, sizeof(pos));
            char *opa = string(d, 1), *xlu = string(d, 1);
            if (!entries)
                continue;
            if (type == 0) {
                PolygonDlist *p = entries;
                p[i].opa = (Gfx *)opa;
                p[i].xlu = (Gfx *)xlu;
            } else {
                PolygonDlist2 *p = entries;
                p[i].pos = (Vec3s){pos[0], pos[1], pos[2]};
                p[i].unk_06 = pos[3];
                p[i].opa = (Gfx *)opa;
                p[i].xlu = (Gfx *)xlu;
            }
        }
        if (h) {
            h->polygon0.num = n;
            h->polygon0.start = entries;
            h->polygon0.end = entries ? (uint8_t *)entries + n * size : NULL;
        }
    } else if (type == 1) {
        uint8_t format = integer(d, 1);
        string(d, 0);
        string(d, 0);
        uint32_t nb = count(d, 255);
        if ((format != 1 && format != 2) || (format == 1 && nb != 1))
            d->error = NANO_ASSET_FORMAT;
        BgImage *backgrounds = reserve(d, nb, sizeof(*backgrounds));
        for (uint32_t i = 0; i < nb; ++i) {
            BgImage b = {0};
            b.unk_00 = integer(d, 2);
            b.id = integer(d, 1);
            b.source = string(d, 1);
            b.unk_0C = integer(d, 4);
            b.tlut = integer(d, 4);
            b.width = integer(d, 2);
            b.height = integer(d, 2);
            b.fmt = integer(d, 1);
            b.siz = integer(d, 1);
            b.mode0 = integer(d, 2);
            b.tlutCount = integer(d, 2);
            if (backgrounds)
                backgrounds[i] = b;
        }
        integer(d, 1);
        PolygonDlist *p = reserve(d, 1, sizeof(*p));
        char *opa = string(d, 1), *xlu = string(d, 1);
        if (p) {
            p->opa = (Gfx *)opa;
            p->xlu = (Gfx *)xlu;
        }
        if (h && !d->error) {
            h->polygon1.format = format;
            h->polygon1.dlist = (Gfx *)p;
            if (format == 1) {
                BgImage *b = backgrounds;
                h->polygon1.single.source = b->source;
                h->polygon1.single.unk_0C = b->unk_0C;
                h->polygon1.single.tlut = (void *)(uintptr_t)b->tlut;
                h->polygon1.single.width = b->width;
                h->polygon1.single.height = b->height;
                h->polygon1.single.fmt = b->fmt;
                h->polygon1.single.siz = b->siz;
                h->polygon1.single.mode0 = b->mode0;
                h->polygon1.single.tlutCount = b->tlutCount;
            } else {
                h->polygon1.multi.count = nb;
                h->polygon1.multi.list = backgrounds;
            }
        }
    } else
        d->error = NANO_ASSET_TYPE;
    return h;
}

static void *scene(struct decode *d)
{
    uint32_t nc = count(d, 256);
    SceneCmd *commands = reserve(d, nc, sizeof(*commands));
    int ended = 0;
    d->asset->count = nc;
    for (uint32_t i = 0; i < nc && !d->error; ++i) {
        uint32_t code = integer(d, 4), n;
        SceneCmd *c = commands ? commands + i : NULL;
        if (c)
            c->base.code = code;
        switch (code) {
        case 0:
        case 1: {
            n = count(d, 255);
            ActorEntry *p = reserve(d, n, sizeof(*p));
            read_bytes(d, p, n * sizeof(*p));
            if (c) {
                c->actorList.num = n;
                c->actorList.segment = p;
            }
            break;
        }
        case 2: {
            /* Unused command in the shipped game. Nonempty camera data
             * requires a defined resource format rather than a ROM pointer. */
            uint8_t size = integer(d, 1);
            uint32_t offset = integer(d, 4);
            if (size || offset)
                d->error = NANO_ASSET_TYPE;
            break;
        }
        case 3:
        case 23: {
            char *name = string(d, 0);
            void *p = dependency(d, name, code == 3 ? TYPE_COL : TYPE_CS);
            if (c)
                c->colHeader.segment = p;
            break;
        }
        case 4: {
            n = count(d, 127);
            RomFile *p = reserve(d, n, sizeof(*p));
            for (uint32_t j = 0; j < n; ++j) {
                char *name = string(d, 0);
                integer(d, 4);
                integer(d, 4);
                /* Resources live outside the old contiguous ROM buffer. */
                if (p)
                    p[j].fileName = name;
            }
            if (c) {
                c->roomList.num = n;
                c->roomList.segment = p;
            }
            break;
        }
        case 5: {
            uint8_t values[4] = {0};
            read_bytes(d, values, 4);
            if (c) {
                c->windSettings.x = values[0];
                c->windSettings.y = values[1];
                c->windSettings.z = values[2];
                c->windSettings.unk_07 = values[3];
            }
            break;
        }
        case 6: {
            n = count(d, 255);
            EntranceEntry *p = reserve(d, n, sizeof(*p));
            read_bytes(d, p, n * sizeof(*p));
            if (c) {
                c->entranceList.data1 = n;
                c->entranceList.segment = p;
            }
            break;
        }
        case 7: {
            uint8_t message = integer(d, 1);
            uint16_t object = integer(d, 2);
            if (message > 2)
                d->error = NANO_ASSET_FORMAT;
            if (c) {
                c->specialFiles.cUpElfMsgNum = message;
                c->specialFiles.keepObjectId = object;
            }
            break;
        }
        case 8: {
            uint8_t flag1 = integer(d, 1);
            uint32_t flag2 = integer(d, 4);
            if (c) {
                c->roomBehavior.gpFlag1 = flag1;
                c->roomBehavior.gpFlag2 = flag2;
            }
            break;
        }
        case 10: {
            void *p = mesh(d);
            if (c)
                c->mesh.segment = p;
            break;
        }
        case 11:
        case 19: {
            n = count(d, 255);
            uint16_t *p = reserve(d, n, sizeof(*p));
            read_bytes(d, p, n * sizeof(*p));
            if (c) {
                c->objectList.num = n;
                c->objectList.segment = p;
            }
            break;
        }
        case 12: {
            n = count(d, 255);
            LightInfo *p = reserve(d, n, sizeof(*p));
            for (uint32_t j = 0; j < n; ++j) {
                LightInfo l = {0};
                l.type = integer(d, 1);
                l.params.point.x = integer(d, 2);
                l.params.point.y = integer(d, 2);
                l.params.point.z = integer(d, 2);
                read_bytes(d, l.params.point.color, 3);
                l.params.point.drawGlow = integer(d, 1);
                l.params.point.radius = integer(d, 2);
                if (p)
                    p[j] = l;
            }
            if (c) {
                c->lightList.num = n;
                c->lightList.segment = p;
            }
            break;
        }
        case 13: {
            n = count(d, 255);
            for (uint32_t j = 0; j < n; ++j) {
                char *name = string(d, 0);
                void *p = dependency(d, name, TYPE_PATH);
                /* The first resource is the complete native Path array,
                 * matching upstream Scene_CommandPathList. */
                if (c && j == 0)
                    c->pathList.segment = p;
            }
            break;
        }
        case 14: {
            n = count(d, 255);
            TransitionActorEntry *p = reserve(d, n, sizeof(*p));
            read_bytes(d, p, n * sizeof(*p));
            if (c) {
                c->transiActorList.num = n;
                c->transiActorList.segment = p;
            }
            break;
        }
        case 15: {
            n = count(d, 255);
            EnvLightSettings *p = reserve(d, n, sizeof(*p));
            read_bytes(d, p, n * sizeof(*p));
            if (c) {
                c->lightSettingList.num = n;
                c->lightSettingList.segment = p;
            }
            break;
        }
        case 16: {
            uint8_t v[3] = {0};
            read_bytes(d, v, 3);
            if (c) {
                c->timeSettings.hour = v[0];
                c->timeSettings.min = v[1];
                c->timeSettings.unk_06 = v[2];
            }
            break;
        }
        case 17: {
            uint8_t v[4] = {0};
            read_bytes(d, v, 4);
            if (c) {
                c->skyboxSettings.data1 = v[0];
                c->skyboxSettings.skyboxId = v[1];
                c->skyboxSettings.unk_05 = v[2];
                c->skyboxSettings.unk_06 = v[3];
            }
            break;
        }
        case 18: {
            uint8_t v[2] = {0};
            read_bytes(d, v, 2);
            if (c) {
                c->skyboxDisables.unk_04 = v[0];
                c->skyboxDisables.unk_05 = v[1];
            }
            break;
        }
        case 20:
            ended = 1;
            break;
        case 21: {
            uint8_t v[3] = {0};
            read_bytes(d, v, 3);
            if (c) {
                c->soundSettings.specId = v[0];
                c->soundSettings.natureAmbienceId = v[1];
                c->soundSettings.seqId = v[2];
            }
            break;
        }
        case 22: {
            uint8_t v = integer(d, 1);
            if (c)
                c->echoSettings.echo = v;
            break;
        }
        case 24: {
            n = count(d, 255);
            char **names = reserve(d, n, sizeof(*names));
            for (uint32_t j = 0; j < n; ++j) {
                char *name = string(d, 1);
                if (names)
                    names[j] = name;
            }
            /* Resolve only the selected age/time/cutscene layer at execution.
             * No other layer can become a dangling native pointer. */
            if (c) {
                c->altHeaders.data1 = n;
                c->altHeaders.segment = names;
            }
            break;
        }
        case 25: {
            uint8_t movement = integer(d, 1);
            uint32_t area = integer(d, 4);
            if (c) {
                c->miscSettings.cameraMovement = movement;
                c->miscSettings.area = area;
            }
            break;
        }
        default:
            d->error = NANO_ASSET_TYPE;
            break;
        }
        if (ended && i != nc - 1)
            d->error = NANO_ASSET_FORMAT;
    }
    if (!ended)
        d->error = NANO_ASSET_FORMAT;
    return commands;
}
