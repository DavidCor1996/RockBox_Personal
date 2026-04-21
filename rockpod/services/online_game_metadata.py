"""Online game metadata lookup using Wikidata's public APIs."""

from __future__ import annotations

import json
import re
import threading
import time
import unicodedata
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


_PUNCT_TRANSLATION = str.maketrans(
    {
        "\u2018": "'",
        "\u2019": "'",
        "\u201a": "'",
        "\u201b": "'",
        "\u2032": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u201e": '"',
        "\u2033": '"',
        "\u2010": "-",
        "\u2011": "-",
        "\u2012": "-",
        "\u2013": "-",
        "\u2014": "-",
        "\u2212": "-",
        "\u00A0": " ",
    }
)

_PLATFORM_KEYWORDS = {
    ".gb": ("game boy",),
    ".gbc": ("game boy color", "game boy"),
}

_INSTANCE_KEYWORDS = ("video game", "game")


class OnlineGameMetadataError(Exception):
    """Raised when game metadata could not be fetched."""


class OnlineGameMetadataNetworkError(OnlineGameMetadataError):
    """Raised when network operations fail."""


def _normalize(value):
    if not value:
        return ""
    value = unicodedata.normalize("NFKC", str(value)).translate(_PUNCT_TRANSLATION)
    value = value.strip().casefold()
    value = re.sub(r"[\[\(](usa|europe|japan|world|rev|v\d|beta|proto)[^)\]]*[\)\]]", " ", value, flags=re.I)
    value = re.sub(r"\b(dx|deluxe|color|version)\b", r" \1 ", value)
    value = re.sub(r"\s*&\s*", " and ", value)
    value = re.sub(r"[`´]", "'", value)
    value = re.sub(r"\s*[-_/]+\s*", " ", value)
    value = re.sub(r"\s+", " ", value)
    return value.strip()


def _search_terms(title):
    raw = str(title or "").strip()
    if not raw:
        return []
    terms = []
    for candidate in (
        raw,
        re.sub(r"\s*[\[(].*?[\])]\s*$", "", raw).strip(),
        re.sub(r"\s+\b(usa|europe|japan|world)\b.*$", "", raw, flags=re.I).strip(),
    ):
        candidate = " ".join(candidate.split())
        if candidate and candidate not in terms:
            terms.append(candidate)
    return terms


class OnlineGameMetadataLookup:
    """Thin client for public video-game metadata lookup."""

    _rate_lock = threading.Lock()
    _last_request_at = 0.0

    def __init__(self, language="en", min_interval_seconds=0.5, timeout=10.0):
        self.language = language or "en"
        self.min_interval_seconds = float(min_interval_seconds or 0.5)
        self.timeout = float(timeout or 10.0)

    def fetch_game_metadata(self, title, platform_hint="", opener=urlopen):
        title = str(title or "").strip()
        if not title:
            return None

        candidate_ids = self._search_entity_ids(title, opener=opener)
        if not candidate_ids:
            return None

        entities = self._fetch_entities(candidate_ids, props="claims|labels|descriptions", opener=opener)
        if not entities:
            return None

        ref_ids = set()
        for entity in entities.values():
            for prop in ("P31", "P400", "P136", "P123", "P178"):
                ref_ids.update(self._claim_entity_ids(entity, prop))
        ref_labels = self._entity_labels(ref_ids, opener=opener)

        best = None
        best_score = 0
        for entity_id in candidate_ids:
            entity = entities.get(entity_id)
            if not entity:
                continue
            score = self._score_entity(entity, title, platform_hint, ref_labels)
            if score > best_score:
                best = entity
                best_score = score

        if not best or best_score < 80:
            return None

        genre = self._first_label(best, "P136", ref_labels)
        publisher = self._first_label(best, "P123", ref_labels)
        developer = self._first_label(best, "P178", ref_labels)
        platform = self._matching_platform(best, platform_hint, ref_labels) or self._first_label(best, "P400", ref_labels)
        description = self._entity_text(best, "descriptions", self.language)
        year = self._year_from_claim(best, "P577")
        entity_id = best.get("id", "")
        return {
            "year": year or "",
            "genre": genre or "",
            "publisher": publisher or "",
            "developer": developer or "",
            "description": description or "",
            "platform": platform or "",
            "source": "wikidata",
            "source_url": f"https://www.wikidata.org/wiki/{entity_id}" if entity_id else "",
            "entity_id": entity_id,
        }

    def _search_entity_ids(self, title, opener=urlopen, limit=8):
        ids = []
        for term in _search_terms(title):
            payload = self._get_json(
                "https://www.wikidata.org/w/api.php",
                params={
                    "action": "wbsearchentities",
                    "format": "json",
                    "language": self.language,
                    "limit": int(limit or 8),
                    "search": term,
                },
                opener=opener,
            )
            for item in payload.get("search", []) or []:
                entity_id = str(item.get("id") or "").strip()
                if entity_id and entity_id not in ids:
                    ids.append(entity_id)
        return ids

    def _fetch_entities(self, entity_ids, props="claims|labels|descriptions", opener=urlopen):
        entity_ids = [str(entity_id or "").strip() for entity_id in entity_ids if str(entity_id or "").strip()]
        if not entity_ids:
            return {}
        payload = self._get_json(
            "https://www.wikidata.org/w/api.php",
            params={
                "action": "wbgetentities",
                "format": "json",
                "languages": self.language,
                "ids": "|".join(entity_ids),
                "props": props,
            },
            opener=opener,
        )
        return dict(payload.get("entities") or {})

    def _entity_labels(self, entity_ids, opener=urlopen):
        entities = self._fetch_entities(entity_ids, props="labels", opener=opener)
        labels = {}
        for entity_id, entity in entities.items():
            label = self._entity_text(entity, "labels", self.language)
            if label:
                labels[entity_id] = label
        return labels

    def _score_entity(self, entity, wanted_title, platform_hint, ref_labels):
        label = _normalize(self._entity_text(entity, "labels", self.language))
        description = _normalize(self._entity_text(entity, "descriptions", self.language))
        wanted = _normalize(wanted_title)
        score = 0

        if label == wanted:
            score += 80
        elif wanted and (wanted in label or label in wanted):
            score += 45

        if any(keyword in description for keyword in _INSTANCE_KEYWORDS):
            score += 25

        instance_labels = [
            _normalize(ref_labels.get(entity_id))
            for entity_id in self._claim_entity_ids(entity, "P31")
        ]
        if any("video game" in label for label in instance_labels if label):
            score += 20

        platform = _normalize(self._matching_platform(entity, platform_hint, ref_labels))
        if platform:
            score += 20
        elif platform_hint and any(keyword in description for keyword in _PLATFORM_KEYWORDS.get(platform_hint, ())):
            score += 10

        if self._year_from_claim(entity, "P577"):
            score += 5

        if description and not any(keyword in description for keyword in _INSTANCE_KEYWORDS):
            score -= 15

        return score

    def _matching_platform(self, entity, platform_hint, ref_labels):
        wanted_keywords = _PLATFORM_KEYWORDS.get(str(platform_hint or "").lower(), ())
        if not wanted_keywords:
            return ""
        for entity_id in self._claim_entity_ids(entity, "P400"):
            label = str(ref_labels.get(entity_id) or "").strip()
            norm = _normalize(label)
            if any(keyword in norm for keyword in wanted_keywords):
                return label
        return ""

    def _first_label(self, entity, prop, ref_labels):
        for entity_id in self._claim_entity_ids(entity, prop):
            label = str(ref_labels.get(entity_id) or "").strip()
            if label:
                return label
        return ""

    @staticmethod
    def _entity_text(entity, field, language):
        row = dict((entity or {}).get(field) or {}).get(language) or {}
        return str(row.get("value") or "").strip()

    @staticmethod
    def _claim_entity_ids(entity, prop):
        ids = []
        for claim in (entity.get("claims") or {}).get(prop) or []:
            mainsnak = dict(claim.get("mainsnak") or {})
            datavalue = dict(mainsnak.get("datavalue") or {})
            value = dict(datavalue.get("value") or {})
            entity_id = str(value.get("id") or "").strip()
            if entity_id and entity_id not in ids:
                ids.append(entity_id)
        return ids

    @staticmethod
    def _year_from_claim(entity, prop):
        for claim in (entity.get("claims") or {}).get(prop) or []:
            mainsnak = dict(claim.get("mainsnak") or {})
            datavalue = dict(mainsnak.get("datavalue") or {})
            value = datavalue.get("value")
            time_value = ""
            if isinstance(value, dict):
                time_value = str(value.get("time") or "")
            elif value is not None:
                time_value = str(value)
            match = re.search(r"([12]\d{3})", time_value)
            if match:
                return match.group(1)
        return ""

    def _get_json(self, base_url, params=None, opener=urlopen):
        url = base_url
        if params:
            url = f"{base_url}?{urlencode(params)}"
        request = Request(url, headers={"User-Agent": "RockPod/1.0"})
        self._respect_rate_limit(self.min_interval_seconds)
        try:
            with opener(request, timeout=self.timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except HTTPError as exc:
            raise OnlineGameMetadataNetworkError(f"HTTP {exc.code}") from exc
        except URLError as exc:
            raise OnlineGameMetadataNetworkError(str(exc.reason)) from exc
        except json.JSONDecodeError as exc:
            raise OnlineGameMetadataNetworkError("Invalid JSON response") from exc

    @classmethod
    def _respect_rate_limit(cls, min_interval_seconds=0.5):
        with cls._rate_lock:
            now = time.monotonic()
            wait = max(0.0, float(min_interval_seconds) - (now - cls._last_request_at))
            if wait > 0:
                time.sleep(wait)
            cls._last_request_at = time.monotonic()
