#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = []
# ///
"""
Cultural Pulse 30d — Scan the internet zeitgeist.

Pulls trending content from 17 free, no-auth sources and produces
a structured cultural pulse report. Defaults to last 30 days.

Usage:
    uv run pulse.py                          # Full default pulse
    uv run pulse.py --days 7                 # Last 7 days
    uv run pulse.py --preset memes           # Meme-focused
    uv run pulse.py --preset global          # Pan-cultural, non-Anglo
    uv run pulse.py --preset culture         # Art, books, wonder, music
    uv run pulse.py --all --summary          # Everything, TL;DR format
    uv run pulse.py --topic "AI" --compact   # Filter + compact
    uv run pulse.py --json                   # Machine-readable
"""

import argparse
import html
import json
import re
import sys
import urllib.request
import urllib.error
import urllib.parse
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta, timezone
from typing import Any

UA = "cultural-pulse-cli/0.2 (github.com/symbient-skills)"
TIMEOUT = 15

DEFAULT_SECTIONS = ["reddit", "hn", "bbc", "wikipedia", "gtrends"]
ALL_SECTIONS = [
    "reddit", "hn", "bbc", "wikipedia", "gtrends",
    "mastodon", "bluesky", "gnews", "lobsters", "lemmy",
    "producthunt", "arxiv",
    "global", "culture", "books",
]

DEFAULT_SUBREDDITS = ["all", "news", "OutOfTheLoop", "technology", "worldnews", "science"]

MEME_SUBREDDITS = ["all", "memes", "dankmemes", "me_irl", "funny", "OutOfTheLoop",
                   "starterpacks", "comedyheaven", "AbsoluteUnits"]

VIRAL_SUBREDDITS = ["popular", "interestingasfuck", "oddlysatisfying", "BrandNewSentence",
                    "rareinsults", "AbsoluteUnits", "NatureIsFuckingLit", "wholesomememes"]

CELEB_SUBREDDITS = ["entertainment", "popculturechat", "Fauxmoi", "BravoRealHousewives",
                    "movies", "television", "Music", "hiphopheads", "popheads",
                    "LiveFromNewYork", "rupaulsdragrace", "90DayFiance"]

PRESETS = {
    "default": {
        "sections": ["reddit", "hn", "bbc", "wikipedia", "gtrends"],
        "description": "Balanced: mass culture + tech + news + search trends + Wikipedia",
    },
    "memes": {
        "sections": ["reddit", "lemmy", "mastodon", "gtrends"],
        "subreddits": MEME_SUBREDDITS,
        "description": "Viral/meme-focused: meme subs + Lemmy + Mastodon + Google Trends",
    },
    "viral": {
        "sections": ["reddit", "gtrends", "mastodon", "bluesky"],
        "subreddits": VIRAL_SUBREDDITS,
        "description": "What's going viral RIGHT NOW across platforms",
    },
    "news": {
        "sections": ["bbc", "gnews", "mastodon", "wikipedia", "global"],
        "description": "Hard news: BBC + Google News + Mastodon + Wikipedia + global sources",
    },
    "global": {
        "sections": ["gtrends", "global", "wikipedia", "mastodon", "reddit"],
        "description": "Pan-cultural: Google Trends (multi-geo) + Al Jazeera/DW/France24/Guardian",
    },
    "tech": {
        "sections": ["hn", "lobsters", "producthunt", "arxiv"],
        "description": "Tech/startup/research: HN + Lobste.rs + Product Hunt + arXiv",
    },
    "culture": {
        "sections": ["culture", "books", "reddit", "mastodon", "wikipedia"],
        "subreddits": ["Art", "DesignPorn", "ArtHistory", "booksuggestions", "Music",
                       "oddlysatisfying", "NatureIsFuckingLit", "interestingasfuck"],
        "description": "Art, books, wonder, nature: Atlas Obscura + Open Culture + OpenLibrary",
    },
    "social": {
        "sections": ["reddit", "mastodon", "bluesky", "lemmy"],
        "description": "Social platforms: Reddit + Mastodon + Bluesky + Lemmy",
    },
    "celeb": {
        "sections": ["reddit", "gtrends", "wikipedia", "gnews", "bbc", "mastodon", "bluesky"],
        "subreddits": CELEB_SUBREDDITS,
        "bbc_sections": ["arts"],
        "gnews_topic": "entertainment",
        "description": "Celebrity/entertainment/trash TV: gossip subs + entertainment news + Wikipedia lookups",
    },
    "deep": {
        "sections": ALL_SECTIONS,
        "description": "Everything: all 17 sources",
    },
}

REDDIT_TIME_MAP = {
    1: "day", 7: "week", 30: "month", 365: "year",
}

BBC_SECTIONS = {
    "news": "https://feeds.bbci.co.uk/news/rss.xml",
    "arts": "https://feeds.bbci.co.uk/news/entertainment_and_arts/rss.xml",
    "tech": "https://feeds.bbci.co.uk/news/technology/rss.xml",
    "science": "https://feeds.bbci.co.uk/news/science_and_environment/rss.xml",
    "world": "https://feeds.bbci.co.uk/news/world/rss.xml",
}

GNEWS_TOPICS = {
    "top": "",
    "technology": "CAAqJggKIiBDQkFTRWdvSUwyMHZNRGRqTVhZU0FtVnVHZ0pIUWlnQVAB",
    "entertainment": "CAAqJggKIiBDQkFTRWdvSUwyMHZNREpxYW5RU0FtVnVHZ0pWVXlnQVAB",
    "science": "CAAqJggKIiBDQkFTRWdvSUwyMHZNRFp0Y1RjU0FtVnVHZ0pWVXlnQVAB",
    "business": "CAAqJggKIiBDQkFTRWdvSUwyMHZNRGx6TVdZU0FtVnVHZ0pWVXlnQVAB",
}

# Google Trends geo codes — sample the planet
GTRENDS_GEOS = ["US", "GB", "JP", "BR", "IN", "DE", "FR", "KR", "NG", "MX"]

GLOBAL_FEEDS = {
    "Al Jazeera": "https://www.aljazeera.com/xml/rss/all.xml",
    "DW": "https://rss.dw.com/rdf/rss-en-all",
    "France 24": "https://www.france24.com/en/rss",
    "Guardian World": "https://www.theguardian.com/world/rss",
    "Guardian Culture": "https://www.theguardian.com/uk/culture/rss",
    "Rest of World": "https://restofworld.org/feed/",
}

CULTURE_FEEDS = {
    "Atlas Obscura": "https://www.atlasobscura.com/feeds/latest",
    "Open Culture": "https://www.openculture.com/feed",
    "Colossal": "https://www.thisiscolossal.com/feed/",
    "NASA APOD": "https://apod.nasa.gov/apod.rss",
    "XKCD": "https://xkcd.com/rss.xml",
    "Bandcamp": "https://daily.bandcamp.com/feed",
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _get(url: str) -> bytes:
    """Fetch URL with User-Agent header."""
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:
        return resp.read()


def _get_json(url: str) -> Any:
    return json.loads(_get(url))


def _get_xml(url: str) -> ET.Element:
    return ET.fromstring(_get(url))


def _age_days(ts: float | int) -> int:
    """Days since a unix timestamp."""
    return (datetime.now(timezone.utc) - datetime.fromtimestamp(ts, tz=timezone.utc)).days


def _age_str(days: int) -> str:
    if days == 0:
        return "today"
    if days == 1:
        return "1d ago"
    return f"{days}d ago"


def _reddit_t(days: int) -> str:
    """Map day count to Reddit's t= parameter."""
    for threshold, value in sorted(REDDIT_TIME_MAP.items()):
        if days <= threshold:
            return value
    return "year"


def _match_topic(text: str, topic: str | None) -> bool:
    """Match topic as a word boundary search (avoids 'AI' matching 'waiter')."""
    if not topic:
        return True
    pattern = r'\b' + re.escape(topic) + r'\b'
    return bool(re.search(pattern, text, re.IGNORECASE))


def _clean_html(text: str) -> str:
    """Strip HTML tags and decode entities."""
    text = re.sub(r'<[^>]+>', '', text)
    return html.unescape(text).strip()


def _rss_items(url: str, source: str, topic: str | None = None, limit: int = 20) -> list[dict]:
    """Generic RSS/Atom feed parser. Works for most feeds."""
    items = []
    try:
        root = _get_xml(url)
        # Try RSS 2.0 first
        for item in root.findall(".//item")[:limit]:
            title_el = item.find("title")
            link_el = item.find("link")
            desc_el = item.find("description")
            if title_el is None or not title_el.text:
                continue
            title = _clean_html(title_el.text)
            if not _match_topic(title, topic):
                continue
            desc = _clean_html(desc_el.text)[:150] if desc_el is not None and desc_el.text else ""
            link = (link_el.text or "").split("?")[0] if link_el is not None else ""
            items.append({
                "title": title,
                "url": link,
                "score": 0,
                "age": -1,
                "source": source,
                "extra": desc,
            })
        if items:
            return items
        # Try Atom
        ns = {"atom": "http://www.w3.org/2005/Atom"}
        for entry in root.findall("atom:entry", ns)[:limit]:
            title_el = entry.find("atom:title", ns)
            link_el = entry.find("atom:link", ns)
            summary_el = entry.find("atom:summary", ns) or entry.find("atom:content", ns)
            if title_el is None or not title_el.text:
                continue
            title = _clean_html(title_el.text)
            if not _match_topic(title, topic):
                continue
            href = link_el.get("href", "") if link_el is not None else ""
            desc = _clean_html(summary_el.text)[:150] if summary_el is not None and summary_el.text else ""
            items.append({
                "title": title,
                "url": href,
                "score": 0,
                "age": -1,
                "source": source,
                "extra": desc,
            })
    except Exception as e:
        print(f"  [{source}] failed: {e}", file=sys.stderr)
    return items


# ---------------------------------------------------------------------------
# Source fetchers
# ---------------------------------------------------------------------------

def fetch_reddit(days: int = 30, subreddits: list[str] | None = None, topic: str | None = None) -> list[dict]:
    subs = subreddits or DEFAULT_SUBREDDITS
    t = _reddit_t(days)
    items = []
    for sub in subs:
        try:
            url = f"https://www.reddit.com/r/{sub}/top.json?t={t}&limit=25"
            data = _get_json(url)
            for c in data["data"]["children"]:
                d = c["data"]
                if not _match_topic(d["title"], topic):
                    continue
                age = _age_days(d["created_utc"])
                if age > days:
                    continue
                items.append({
                    "title": d["title"],
                    "url": "https://www.reddit.com" + d["permalink"],
                    "score": d["score"],
                    "age": age,
                    "source": f"r/{d['subreddit']}",
                    "extra": f"{d['num_comments']} comments | {d['upvote_ratio']:.0%} upvoted",
                })
        except Exception as e:
            print(f"  [reddit] r/{sub} failed: {e}", file=sys.stderr)
    seen = {}
    for item in items:
        key = item["title"]
        if key not in seen or item["score"] > seen[key]["score"]:
            seen[key] = item
    result = sorted(seen.values(), key=lambda x: x["score"], reverse=True)
    return result[:30]


def fetch_hn(days: int = 30, topic: str | None = None) -> list[dict]:
    items = []
    try:
        ids = _get_json("https://hacker-news.firebaseio.com/v0/topstories.json")[:50]
        for item_id in ids:
            try:
                item = _get_json(f"https://hacker-news.firebaseio.com/v0/item/{item_id}.json")
                if not item or not item.get("title"):
                    continue
                if not _match_topic(item["title"], topic):
                    continue
                age = _age_days(item.get("time", 0))
                if age > days:
                    continue
                items.append({
                    "title": item["title"],
                    "url": item.get("url", f"https://news.ycombinator.com/item?id={item_id}"),
                    "score": item.get("score", 0),
                    "age": age,
                    "source": "Hacker News",
                    "extra": f"{item.get('descendants', 0)} comments",
                })
            except Exception:
                continue
    except Exception as e:
        print(f"  [hn] failed: {e}", file=sys.stderr)
    return sorted(items, key=lambda x: x["score"], reverse=True)[:20]


def fetch_bbc(days: int = 30, topic: str | None = None, bbc_sections: list[str] | None = None) -> list[dict]:
    items = []
    active = {k: v for k, v in BBC_SECTIONS.items() if not bbc_sections or k in bbc_sections}
    for section, url in active.items():
        for item in _rss_items(url, f"BBC {section.title()}", topic, limit=10):
            items.append(item)
    return items[:25]


def fetch_wikipedia(days: int = 30, topic: str | None = None) -> list[dict]:
    items = []
    try:
        dt = datetime.now() - timedelta(days=2)
        url = (f"https://wikimedia.org/api/rest_v1/metrics/pageviews/top/"
               f"en.wikipedia.org/all-access/{dt.year}/{dt.month:02d}/{dt.day:02d}")
        data = _get_json(url)
        skip = {"Main_Page", "Special:Search", "-", "Wikipedia:Featured_pictures"}
        articles = data["items"][0]["articles"]
        for a in articles[:200]:
            name = a["article"]
            if any(name.startswith(s) for s in skip) or ":" in name:
                continue
            readable = name.replace("_", " ")
            if not _match_topic(readable, topic):
                continue
            items.append({
                "title": readable,
                "url": f"https://en.wikipedia.org/wiki/{name}",
                "score": a["views"],
                "age": 2,
                "source": "Wikipedia",
                "extra": f"{a['views']:,} views (rank #{a['rank']})",
            })
    except Exception as e:
        print(f"  [wikipedia] failed: {e}", file=sys.stderr)
    return items[:30]


def fetch_gtrends(days: int = 30, topic: str | None = None) -> list[dict]:
    """Google Trends daily trending searches — multi-geo for global snapshot."""
    items = []
    for geo in GTRENDS_GEOS:
        try:
            url = f"https://trends.google.com/trending/rss?geo={geo}"
            root = _get_xml(url)
            ns = {"ht": "https://trends.google.com/trending/rss"}
            for item in root.findall(".//item")[:5]:
                title_el = item.find("title")
                if title_el is None or not title_el.text:
                    continue
                title = title_el.text
                if not _match_topic(title, topic):
                    continue
                traffic_el = item.find("ht:approx_traffic", ns)
                traffic = traffic_el.text if traffic_el is not None else ""
                link_el = item.find("link")
                news_items = item.findall("ht:news_item", ns)
                news_title = ""
                news_url = ""
                if news_items:
                    nt = news_items[0].find("ht:news_item_title", ns)
                    nu = news_items[0].find("ht:news_item_url", ns)
                    news_title = _clean_html(nt.text) if nt is not None and nt.text else ""
                    news_url = nu.text if nu is not None and nu.text else ""
                # Parse traffic like "200,000+" into int
                score = 0
                if traffic:
                    digits = re.sub(r'[^\d]', '', traffic)
                    score = int(digits) if digits else 0
                items.append({
                    "title": title,
                    "url": news_url or (link_el.text if link_el is not None else ""),
                    "score": score,
                    "age": 0,
                    "source": f"Google Trends ({geo})",
                    "extra": f"{traffic} searches" + (f" — {news_title}" if news_title else ""),
                })
        except Exception as e:
            print(f"  [gtrends] {geo} failed: {e}", file=sys.stderr)
    return sorted(items, key=lambda x: x["score"], reverse=True)[:30]


def fetch_mastodon(days: int = 30, topic: str | None = None) -> list[dict]:
    items = []
    try:
        links = _get_json("https://mastodon.social/api/v1/trends/links?limit=20")
        for link in links:
            if not _match_topic(link.get("title", ""), topic):
                continue
            items.append({
                "title": link.get("title", ""),
                "url": link.get("url", ""),
                "score": 0,
                "age": -1,
                "source": "Mastodon",
                "extra": _clean_html(link.get("description", ""))[:150],
            })
    except Exception as e:
        print(f"  [mastodon] failed: {e}", file=sys.stderr)
    try:
        tags = _get_json("https://mastodon.social/api/v1/trends/tags?limit=10")
        for tag in tags:
            name = tag.get("name", "")
            if not _match_topic(name, topic):
                continue
            uses = int(tag.get("history", [{}])[0].get("uses", 0)) if tag.get("history") else 0
            items.append({
                "title": f"#{name}",
                "url": f"https://mastodon.social/tags/{name}",
                "score": uses,
                "age": 0,
                "source": "Mastodon tags",
                "extra": f"{uses} uses today",
            })
    except Exception as e:
        print(f"  [mastodon tags] failed: {e}", file=sys.stderr)
    return items


def fetch_bluesky(days: int = 30, topic: str | None = None) -> list[dict]:
    items = []
    try:
        data = _get_json("https://public.api.bsky.app/xrpc/app.bsky.unspecced.getTrendingTopics")
        for t in data.get("topics", []):
            name = t.get("topic", "")
            if not _match_topic(name, topic):
                continue
            items.append({
                "title": name,
                "url": t.get("link", ""),
                "score": 0,
                "age": 0,
                "source": "Bluesky",
                "extra": "trending topic",
            })
    except Exception as e:
        print(f"  [bluesky] failed: {e}", file=sys.stderr)
    return items


def fetch_gnews(days: int = 30, topic: str | None = None, gnews_topic: str | None = None) -> list[dict]:
    try:
        if topic:
            url = f"https://news.google.com/rss/search?q={urllib.parse.quote(topic)}&hl=en-US&gl=US&ceid=US:en"
        elif gnews_topic and gnews_topic != "top" and gnews_topic in GNEWS_TOPICS:
            code = GNEWS_TOPICS[gnews_topic]
            url = f"https://news.google.com/rss/topics/{code}?hl=en-US&gl=US&ceid=US:en"
        else:
            url = "https://news.google.com/rss?hl=en-US&gl=US&ceid=US:en"
        items = []
        root = _get_xml(url)
        for item in root.findall(".//item")[:25]:
            title_el = item.find("title")
            link_el = item.find("link")
            source_el = item.find("source")
            if title_el is None:
                continue
            items.append({
                "title": title_el.text or "",
                "url": link_el.text if link_el is not None else "",
                "score": 0,
                "age": -1,
                "source": f"Google News ({source_el.text})" if source_el is not None else "Google News",
                "extra": "",
            })
        return items
    except Exception as e:
        print(f"  [gnews] failed: {e}", file=sys.stderr)
        return []


def fetch_lobsters(days: int = 30, topic: str | None = None) -> list[dict]:
    items = []
    try:
        data = _get_json("https://lobste.rs/hottest.json")
        for item in data:
            if not _match_topic(item["title"], topic):
                continue
            items.append({
                "title": item["title"],
                "url": item.get("url", "") or item.get("short_id_url", ""),
                "score": item.get("score", 0),
                "age": -1,
                "source": "Lobste.rs",
                "extra": f"{item.get('comment_count', 0)} comments | tags: {', '.join(item.get('tags', []))}",
            })
    except Exception as e:
        print(f"  [lobsters] failed: {e}", file=sys.stderr)
    return items


def fetch_lemmy(days: int = 30, topic: str | None = None) -> list[dict]:
    items = []
    try:
        sort = "TopMonth" if days <= 30 else "TopYear"
        url = f"https://lemmy.world/api/v3/post/list?sort={sort}&limit=25"
        data = _get_json(url)
        for p in data["posts"]:
            title = p["post"]["name"]
            if not _match_topic(title, topic):
                continue
            items.append({
                "title": title,
                "url": p["post"].get("url", "") or p["post"].get("ap_id", ""),
                "score": p["counts"]["score"],
                "age": -1,
                "source": f"Lemmy c/{p['community']['name']}",
                "extra": f"{p['counts']['comments']} comments",
            })
    except Exception as e:
        print(f"  [lemmy] failed: {e}", file=sys.stderr)
    return items[:20]


def fetch_producthunt(days: int = 30, topic: str | None = None) -> list[dict]:
    return _rss_items("https://www.producthunt.com/feed", "Product Hunt", topic, limit=15)


def fetch_arxiv(days: int = 30, topic: str | None = None) -> list[dict]:
    items = []
    try:
        query = topic or "artificial intelligence"
        url = (f"http://export.arxiv.org/api/query?"
               f"search_query=all:{urllib.parse.quote(query)}"
               f"&max_results=15&sortBy=submittedDate&sortOrder=descending")
        root = _get_xml(url)
        ns = {"atom": "http://www.w3.org/2005/Atom"}
        for entry in root.findall("atom:entry", ns):
            title_el = entry.find("atom:title", ns)
            link_el = entry.find("atom:id", ns)
            summary_el = entry.find("atom:summary", ns)
            if title_el is None:
                continue
            title = " ".join((title_el.text or "").split())
            items.append({
                "title": title,
                "url": link_el.text if link_el is not None else "",
                "score": 0,
                "age": -1,
                "source": "arXiv",
                "extra": " ".join((summary_el.text or "").split())[:150] if summary_el is not None else "",
            })
    except Exception as e:
        print(f"  [arxiv] failed: {e}", file=sys.stderr)
    return items


def fetch_global(days: int = 30, topic: str | None = None) -> list[dict]:
    """Global non-Anglo news: Al Jazeera, DW, France 24, Guardian, Rest of World."""
    items = []
    for source, url in GLOBAL_FEEDS.items():
        for item in _rss_items(url, source, topic, limit=8):
            items.append(item)
    return items[:30]


def fetch_culture(days: int = 30, topic: str | None = None) -> list[dict]:
    """Wonder/art/science: Atlas Obscura, Open Culture, Colossal, NASA, XKCD, Bandcamp."""
    items = []
    for source, url in CULTURE_FEEDS.items():
        for item in _rss_items(url, source, topic, limit=5):
            items.append(item)
    return items[:25]


def fetch_books(days: int = 30, topic: str | None = None) -> list[dict]:
    """OpenLibrary trending books."""
    items = []
    for period in ["daily", "weekly"]:
        try:
            data = _get_json(f"https://openlibrary.org/trending/{period}.json?limit=10")
            for work in data.get("works", []):
                title = work.get("title", "")
                if not _match_topic(title, topic):
                    continue
                author = work.get("author_name", ["Unknown"])[0] if work.get("author_name") else "Unknown"
                key = work.get("key", "")
                items.append({
                    "title": title,
                    "url": f"https://openlibrary.org{key}" if key else "",
                    "score": 0,
                    "age": -1,
                    "source": f"OpenLibrary ({period})",
                    "extra": f"by {author}",
                })
        except Exception as e:
            print(f"  [books] {period} failed: {e}", file=sys.stderr)
    # Deduplicate by title
    seen = {}
    for item in items:
        if item["title"] not in seen:
            seen[item["title"]] = item
    return list(seen.values())[:15]


# ---------------------------------------------------------------------------
# Registry
# ---------------------------------------------------------------------------

FETCHERS = {
    "reddit": fetch_reddit,
    "hn": fetch_hn,
    "bbc": fetch_bbc,
    "wikipedia": fetch_wikipedia,
    "gtrends": fetch_gtrends,
    "mastodon": fetch_mastodon,
    "bluesky": fetch_bluesky,
    "gnews": fetch_gnews,
    "lobsters": fetch_lobsters,
    "lemmy": fetch_lemmy,
    "producthunt": fetch_producthunt,
    "arxiv": fetch_arxiv,
    "global": fetch_global,
    "culture": fetch_culture,
    "books": fetch_books,
}

SECTION_LABELS = {
    "reddit": "Reddit — Mass Culture",
    "hn": "Hacker News — Tech Culture",
    "bbc": "BBC News — World Baseline",
    "wikipedia": "Wikipedia — What People Are Looking Up",
    "gtrends": "Google Trends — What The World Is Searching",
    "mastodon": "Mastodon — Fediverse Pulse",
    "bluesky": "Bluesky — Social Trending",
    "gnews": "Google News — Editorial Selection",
    "lobsters": "Lobste.rs — Programming Culture",
    "lemmy": "Lemmy — Community Pulse",
    "producthunt": "Product Hunt — Launches",
    "arxiv": "arXiv — Research Frontier",
    "global": "Global News — Al Jazeera, DW, France 24, Guardian, Rest of World",
    "culture": "Culture & Wonder — Atlas Obscura, Open Culture, Colossal, NASA, XKCD, Bandcamp",
    "books": "Trending Books — OpenLibrary",
}


# ---------------------------------------------------------------------------
# Output formatters
# ---------------------------------------------------------------------------

def format_markdown(results: dict[str, list[dict]], days: int, compact: bool = False) -> str:
    now = datetime.now().strftime("%Y-%m-%d %H:%M")
    lines = [
        f"# Cultural Pulse — Last {days} Days",
        f"*Generated {now}*\n",
    ]
    total = sum(len(v) for v in results.values())
    active = [s for s in results if results[s]]
    lines.append(f"**{total} items** from **{len(active)} sources**\n")
    lines.append("---\n")

    for section, items in results.items():
        label = SECTION_LABELS.get(section, section)
        lines.append(f"## {label}")
        if not items:
            lines.append("*No results*\n")
            continue
        lines.append("")
        for i, item in enumerate(items, 1):
            score_str = f" [{item['score']:,}]" if item["score"] else ""
            age_str = f" ({_age_str(item['age'])})" if item["age"] >= 0 else ""
            lines.append(f"{i}. **{item['title']}**{score_str}{age_str}")
            if not compact:
                lines.append(f"   {item['source']} | {item['url']}")
                if item["extra"]:
                    lines.append(f"   _{item['extra']}_")
            lines.append("")
        lines.append("---\n")

    return "\n".join(lines)


def format_summary(results: dict[str, list[dict]], days: int) -> str:
    """Standalone TL;DR report — readable without any other context."""
    now = datetime.now().strftime("%Y-%m-%d %H:%M")
    lines = [
        f"# Cultural Pulse — Last {days} Days",
        f"*Generated {now}*\n",
    ]

    all_items: list[dict] = []
    for items in results.values():
        all_items.extend(items)

    total = len(all_items)
    active_sources = [s for s in results if results[s]]
    lines.append(f"**{total} items** scanned from **{len(active_sources)} sources**: "
                 + ", ".join(SECTION_LABELS.get(s, s).split(" — ")[0] for s in active_sources))
    lines.append("")

    # --------------- TOP STORIES (with descriptions and links) ---------------
    scored = sorted([i for i in all_items if i["score"] > 0],
                    key=lambda x: x["score"], reverse=True)

    if scored:
        lines.append("---")
        lines.append("## Top Stories by Engagement\n")
        for item in scored[:15]:
            lines.append(f"**{item['title']}**")
            score_label = f"{item['score']:,}"
            parts = [item['source'], f"score: {score_label}"]
            if item["age"] >= 0:
                parts.append(_age_str(item["age"]))
            lines.append(f"_{' | '.join(parts)}_")
            if item["extra"]:
                lines.append(f"> {item['extra']}")
            if item["url"]:
                lines.append(f"[Link]({item['url']})")
            lines.append("")

    # --------------- UNSCORED HIGHLIGHTS (BBC, global, culture — no scores) ---------------
    unscored = [i for i in all_items if i["score"] == 0 and i["extra"]]
    if unscored:
        lines.append("---")
        lines.append("## Headlines & Highlights (editorial sources)\n")
        seen_titles = set()
        count = 0
        for item in unscored:
            if item["title"] in seen_titles or count >= 20:
                continue
            seen_titles.add(item["title"])
            count += 1
            link_part = f" — [link]({item['url']})" if item["url"] else ""
            lines.append(f"- **{item['title']}** ({item['source']}){link_part}")
            if item["extra"]:
                lines.append(f"  _{item['extra'][:120]}_")
        lines.append("")

    # --------------- CROSS-SOURCE THEMES ---------------
    lines.append("---")
    lines.append("## Recurring Themes (appearing across 2+ sources)\n")

    word_sources: dict[str, set[str]] = {}
    word_titles: dict[str, list[str]] = {}
    stop_words = {"this", "that", "with", "from", "will", "have", "been", "their", "they",
                  "what", "when", "where", "which", "about", "after", "before", "into",
                  "more", "most", "some", "than", "them", "then", "were", "your", "just",
                  "over", "also", "back", "even", "only", "very", "here", "much", "many",
                  "does", "like", "being", "other", "could", "would", "should", "people",
                  "first", "said", "says", "http", "https", "still", "ever", "make",
                  "made", "take", "took", "come", "came", "going", "goes", "gone",
                  "gets", "give", "gave", "good", "best", "want", "need", "know",
                  "knew", "look", "find", "found", "show", "tell", "told", "keep",
                  "kept", "left", "right", "long", "last", "next", "same", "turn",
                  "help", "part", "call", "down", "work", "used", "year", "years",
                  "time", "life", "well", "open", "high", "real", "home"}
    for item in all_items:
        words = re.findall(r'\b[a-zA-Z]{4,}\b', item["title"].lower())
        source_key = item["source"].split("/")[-1].split("(")[0].strip().split()[0]
        for word in set(words):
            if word not in stop_words:
                if word not in word_sources:
                    word_sources[word] = set()
                    word_titles[word] = []
                word_sources[word].add(source_key)
                if len(word_titles[word]) < 3:
                    word_titles[word].append(item["title"][:80])

    cross_source = {w: (srcs, word_titles[w])
                    for w, srcs in word_sources.items() if len(srcs) >= 2}
    if cross_source:
        for word, (srcs, titles) in sorted(cross_source.items(), key=lambda x: -len(x[1][0]))[:12]:
            lines.append(f"**{word}** (across {', '.join(sorted(srcs))})")
            for t in titles[:2]:
                lines.append(f"  - \"{t}\"")
            lines.append("")

    # --------------- SOURCE COVERAGE TABLE ---------------
    lines.append("---")
    lines.append("## Source Coverage\n")
    lines.append("| Source | Items | Top Story |")
    lines.append("|--------|-------|-----------|")
    for section, items in results.items():
        label = SECTION_LABELS.get(section, section).split(" — ")[0]
        count = len(items)
        if count:
            top = items[0]["title"][:50] + ("..." if len(items[0]["title"]) > 50 else "")
            lines.append(f"| {label} | {count} | {top} |")
        else:
            lines.append(f"| {label} | 0 | — |")

    lines.append(f"\n**Total: {total} items across {len(active_sources)} sources**")
    return "\n".join(lines)


def format_json_output(results: dict[str, list[dict]], days: int) -> str:
    return json.dumps({"days": days, "generated": datetime.now().isoformat(), "sections": results}, indent=2)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Cultural Pulse 30d — scan the internet zeitgeist",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
sections (17 total):
  DEFAULT: reddit, hn, bbc, wikipedia, gtrends
  OPTIONAL: mastodon, bluesky, gnews, lobsters, lemmy, producthunt, arxiv,
            global, culture, books

presets:
  default   Balanced pulse (5 sources)
  memes     Meme subs + Lemmy + Mastodon + Google Trends
  viral     What's going viral RIGHT NOW
  news      Hard news (BBC, Google News, global, Wikipedia)
  global    Pan-cultural (multi-geo trends, Al Jazeera, DW, etc.)
  tech      HN + Lobste.rs + Product Hunt + arXiv
  culture   Art, books, wonder, nature, music
  social    Reddit + Mastodon + Bluesky + Lemmy
  deep      All 17 sources

examples:
  uv run pulse.py                          # full default pulse
  uv run pulse.py --preset global          # pan-cultural
  uv run pulse.py --preset culture         # art/books/wonder
  uv run pulse.py --all --summary          # everything, TL;DR
  uv run pulse.py --topic "Olympics"       # filter by topic
        """,
    )
    parser.add_argument("--days", type=int, default=30, help="Time window in days (default: 30)")
    parser.add_argument("--preset", type=str, default=None,
                        choices=list(PRESETS.keys()),
                        help="Use a predefined section combo")
    parser.add_argument("--sections", type=str, default=None,
                        help="Comma-separated list of sections")
    parser.add_argument("--all", action="store_true", help="Include all sections")
    parser.add_argument("--topic", type=str, default=None, help="Filter results by topic keyword")
    parser.add_argument("--gnews-topic", type=str, default=None,
                        choices=list(GNEWS_TOPICS.keys()),
                        help="Google News topic")
    parser.add_argument("--subreddits", type=str, default=None,
                        help="Comma-separated subreddits")
    parser.add_argument("--bbc-sections", type=str, default=None,
                        help="Comma-separated BBC sections (news,arts,tech,science,world)")
    parser.add_argument("--summary", action="store_true", help="Standalone TL;DR report")
    parser.add_argument("--compact", action="store_true", help="Compact output (titles only)")
    parser.add_argument("--json", action="store_true", help="JSON output")
    parser.add_argument("--output", "-o", type=str, default=None, help="Write output to file")
    parser.add_argument("--list-sections", action="store_true", help="List all sections and presets")
    parser.add_argument("--limit", type=int, default=None, help="Max items per section")

    args = parser.parse_args()

    if args.list_sections:
        print("Available sections:\n")
        print("DEFAULT:")
        for s in DEFAULT_SECTIONS:
            print(f"  {s:15s} {SECTION_LABELS.get(s, '')}")
        print("\nOPTIONAL:")
        for s in ALL_SECTIONS:
            if s not in DEFAULT_SECTIONS:
                print(f"  {s:15s} {SECTION_LABELS.get(s, '')}")
        print("\nPRESETS (--preset NAME):")
        for name, preset in PRESETS.items():
            secs = ", ".join(preset["sections"])
            print(f"  {name:15s} {preset['description']}")
            print(f"  {' ':15s} [{secs}]")
        sys.exit(0)

    subreddits = args.subreddits.split(",") if args.subreddits else None
    bbc_sections = args.bbc_sections.split(",") if args.bbc_sections else None

    gnews_topic = args.gnews_topic

    if args.preset:
        preset = PRESETS[args.preset]
        sections = preset["sections"]
        if not subreddits and "subreddits" in preset:
            subreddits = preset["subreddits"]
        if not bbc_sections and "bbc_sections" in preset:
            bbc_sections = preset["bbc_sections"]
        if not gnews_topic and "gnews_topic" in preset:
            gnews_topic = preset["gnews_topic"]
    elif args.all:
        sections = ALL_SECTIONS
    elif args.sections:
        sections = [s.strip() for s in args.sections.split(",")]
        invalid = [s for s in sections if s not in FETCHERS]
        if invalid:
            print(f"Unknown sections: {', '.join(invalid)}", file=sys.stderr)
            print(f"Available: {', '.join(ALL_SECTIONS)}", file=sys.stderr)
            sys.exit(1)
    else:
        sections = DEFAULT_SECTIONS

    # Parallel fetch — all sources concurrently via thread pool
    from concurrent.futures import ThreadPoolExecutor, as_completed

    def _fetch_one(section: str) -> tuple[str, list[dict]]:
        print(f"Fetching {section}...", file=sys.stderr)
        fetcher = FETCHERS[section]
        kwargs: dict[str, Any] = {"days": args.days, "topic": args.topic}
        if section == "reddit" and subreddits:
            kwargs["subreddits"] = subreddits
        if section == "bbc" and bbc_sections:
            kwargs["bbc_sections"] = bbc_sections
        if section == "gnews" and gnews_topic:
            kwargs["gnews_topic"] = gnews_topic
        try:
            result = fetcher(**kwargs)
        except Exception as e:
            print(f"  {section} failed: {e}", file=sys.stderr)
            result = []
        if args.limit:
            result = result[:args.limit]
        print(f"  {section}: {len(result)} items", file=sys.stderr)
        return section, result

    results: dict[str, list[dict]] = {}
    with ThreadPoolExecutor(max_workers=len(sections)) as pool:
        futures = {pool.submit(_fetch_one, s): s for s in sections}
        for future in as_completed(futures):
            section, items = future.result()
            results[section] = items

    if args.json:
        output = format_json_output(results, args.days)
    elif args.summary:
        output = format_summary(results, args.days)
    else:
        output = format_markdown(results, args.days, compact=args.compact)

    if args.output:
        with open(args.output, "w") as f:
            f.write(output)
        print(f"\nWritten to {args.output}", file=sys.stderr)
    else:
        print(output)


if __name__ == "__main__":
    main()
