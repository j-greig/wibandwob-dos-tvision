# Cultural Pulse 30d — Status

**Status:** BUILT and working. 11 sources, 6 presets, 4 output formats.

## Parking Lot / Future Ideas

### Sources to investigate
- **Threads API** — Meta may offer a public API in future (currently no public trending endpoint)
- **TikTok** — No free public API for trending content
- **YouTube Trending** — No free API without Google API key (quota-limited free tier exists)
- **Spotify Charts** — Would need scraping, no public API
- **GitHub Trending** — Unofficial endpoint exists (`github.com/trending`) but needs HTML scraping
- **NPR RSS** — Works (`feeds.npr.org/1001/rss.xml`) but only 10 items, low value vs BBC
- **Tildes.net** — Atom feed works, small community, could add as niche source

### Feature ideas
- **Parallel fetching** — Use `concurrent.futures.ThreadPoolExecutor` for speed (currently sequential)
- **Caching** — Cache responses for N minutes to avoid hammering sources during iteration
- **Historical comparison** — Compare today's pulse vs 7/30 days ago ("rising" vs "fading" themes)
- **Sentiment overlay** — Basic positive/negative word matching on titles
- **Category auto-tagging** — Classify items into politics/tech/culture/science/entertainment
- **Cross-source deduplication** — Same story across BBC/Reddit/Google News should merge
- **Custom subreddit presets** — e.g. `--subreddit-preset art` for r/Art, r/DesignPorn, r/ArtHistory

### Known limitations
- Wikipedia pageviews have ~2 day lag (uses day-before-yesterday)
- Google News links are redirect URLs, not direct article URLs
- Reddit rate limit: ~10 req/min without auth (6 subreddits = 6 requests)
- Mastodon trends are instance-specific (only mastodon.social, not the full fediverse)
- Bluesky `getTrendingTopics` is under `unspecced` namespace — could break
- Product Hunt Atom feed has no engagement metrics
- arXiv defaults to "artificial intelligence" if no topic specified

### iMac sync
- Check if the original skill on the iMac had features not replicated here
- May have had different source selection or output format
