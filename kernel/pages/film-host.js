/* film-host.js: many WibWob-DOS films on one page, one engine download, a bounded number of live players.
 * Format and lifecycle: docs/film-file.md (Split delivery). Built into a page by tools/films_demo.py.
 *
 *   <div data-film="films/polka.film.json"></div>   ...
 *   <script src="film-host.js" data-engine="engine/engine.json" data-max="2"></script>
 *
 * Each placeholder shows the film's poster (its first frame as text, from the manifest) until it scrolls into view or
 * is clicked; then a player mounts. A player that has been out of view for data-idle-ms (default 4000), or is the
 * least recently seen when another must mount and data-max (default 2) are live, is unmounted: its stage time is
 * kept, its WebGL context is released (WEBGL_lose_context) and its iframe removed, which frees the wasm instance,
 * its memory and its audio. Scrolled back to, it mounts again at the time it left.
 *
 * Shared: the engine files (engine.json, the wasm, the glue, the shell) are fetched once, and the wasm is compiled
 * once into one WebAssembly.Module. Per player: an iframe (its own document and globals, because the shell is a
 * whole-page script), its own canvas and WebGL context, its own wasm instance and memory, its own audio context.
 * One download and one compiled module, not one runtime.
 */
(function () {
  'use strict';
  var me = document.currentScript;
  var cfg = {
    engine: (me && me.dataset.engine) || 'engine/engine.json',
    max: Math.max(1, parseInt((me && me.dataset.max) || '2', 10) || 2),
    idleMs: parseInt((me && me.dataset.idleMs) || '4000', 10),
    threshold: parseFloat((me && me.dataset.threshold) || '0.5'),
    autostart: !me || me.dataset.autostart !== '0'
  };
  var ABI = 1;
  var host = window.wwdosFilmHost = {
    cfg: cfg, players: {}, live: [], module: null,
    stats: { engineFetches: {}, mounts: 0, unmounts: 0, maxLive: 0, refused: [], lost: 0, log: [] }
  };
  var enginePromise = null, nextId = 1;

  function log(s) { host.stats.log.push(Math.round(performance.now()) + ' ' + s); }
  function fetchOnce(url, kind) {
    host.stats.engineFetches[kind] = (host.stats.engineFetches[kind] || 0) + 1;
    return fetch(url).then(function (r) { if (!r.ok) throw new Error(url + ': HTTP ' + r.status); return r; });
  }
  function hex(buf) { return Array.prototype.map.call(new Uint8Array(buf), function (b) { return (b < 16 ? '0' : '') + b.toString(16); }).join(''); }
  function checkHash(buf, want, what) {
    if (!want || !(window.crypto && crypto.subtle)) return Promise.resolve(buf);
    return crypto.subtle.digest('SHA-256', buf).then(function (d) {
      if ('sha256:' + hex(d) !== want) throw new Error(what + ': hash does not match');
      return buf;
    });
  }

  // ---- the engine: fetched and compiled once, on the first mount ----
  function engine() {
    if (enginePromise) return enginePromise;
    var base = new URL(cfg.engine, location.href);
    enginePromise = fetchOnce(base, 'engine.json').then(function (r) { return r.json(); }).then(function (info) {
      if (info.abi !== ABI) throw new Error('engine.json abi: ' + info.abi + ', this host knows ' + ABI);
      var u = function (k) { return new URL(info[k].file, base); };
      return Promise.all([
        fetchOnce(u('wasm'), 'wasm').then(function (r) { return r.arrayBuffer(); })
          .then(function (b) { return checkHash(b, info.wasm.sha256, 'engine wasm'); })
          .then(function (b) { return WebAssembly.compile(b); }),
        fetchOnce(u('glue'), 'glue').then(function (r) { return r.text(); }),
        fetchOnce(u('shell'), 'shell').then(function (r) { return r.text(); })
      ]).then(function (a) {
        host.module = a[0];
        log('engine compiled');
        return { info: info, module: a[0], glue: a[1], shell: a[2] };
      });
    });
    return enginePromise;
  }

  // ---- a film's manifest: fetched once per film (posters need it before the engine) ----
  function manifest(p) {
    if (p.manifestPromise) return p.manifestPromise;
    var url = new URL(p.src, location.href);
    p.manifestPromise = fetch(url).then(function (r) { if (!r.ok) throw new Error(p.src + ': HTTP ' + r.status); return r.json(); })
      .then(function (m) {
        if (m.kind !== 'wwdos-film') throw new Error('kind: not a wwdos-film');
        if (m.format !== 1) throw new Error('format: this film is format ' + m.format + '; this host reads format 1');
        if (!m.runtime || m.runtime.abi !== ABI) throw new Error('runtime.abi: this film needs ABI ' + (m.runtime && m.runtime.abi) + '; this host provides ' + ABI);
        return m;
      });
    return p.manifestPromise;
  }
  function sounds(p, m) {
    if (p.soundPromise) return p.soundPromise;
    var base = new URL(p.src, location.href), names = Object.keys(m.sounds || {});
    p.soundPromise = Promise.all(names.map(function (n) {
      var s = m.sounds[n];
      if (s.data) return 'data:' + s.mime + ';base64,' + s.data;
      return fetch(new URL(s.url, base)).then(function (r) { return r.arrayBuffer(); }).then(function (b) {
        if (b.byteLength !== s.bytes) throw new Error('sounds["' + n + '"].bytes: ' + s.bytes + ' in the manifest, ' + b.byteLength + ' fetched');
        return checkHash(b, s.sha256, 'sounds["' + n + '"].sha256');
      }).then(function (b) {
        var u8 = new Uint8Array(b), bin = '', CH = 0x8000;
        for (var i = 0; i < u8.length; i += CH) bin += String.fromCharCode.apply(null, u8.subarray(i, i + CH));
        return 'data:' + s.mime + ';base64,' + btoa(bin);
      });
    })).then(function (urls) { var o = {}; names.forEach(function (n, i) { o[n] = urls[i]; }); return o; });
    return p.soundPromise;
  }
  // window.wwdosScene as tools/scene.py writes it (key order included), from the manifest
  function sceneData(m, snd) {
    var d = { files: {}, scene: m.scene }, st = m.settings || {};
    m.boot_order.forEach(function (n) { d.files[n] = n === 'stage.txt' ? m.stage : m.files[n]; });
    if (st.notes !== undefined) d.notes = st.notes;
    if (st.asleep !== undefined) d.asleep = st.asleep;
    if (Object.keys(snd).length) d.sounds = snd;
    if (st.sfx !== undefined) d.sfx = st.sfx;
    if (st.volume !== undefined) d.volume = st.volume;
    return d;
  }
  // The player's document: the shell with the film's atlas, scene and the shared glue. A small script before the
  // shell takes the compiled module from this page and hands it to the shell's own instantiateWasm hook (which expects
  // the {module, instance} that instantiating bytes gives), and keeps the shell's canvas.focus() from scrolling this page.
  function playerDoc(E, m, snd, id) {
    var scene = '<script>window.wwdosScene = ' + JSON.stringify(sceneData(m, snd)).replace(/<\//g, '<\\/') + ';</script>\n';
    var hook = '<script>(function(){var H=parent.wwdosFilmHost,P=H.players[' + id + '];window.wwdosOpts=P.opts;' +
      'window.wwdosWasm=H.module;var F=HTMLElement.prototype.focus;HTMLElement.prototype.focus=function(o){F.call(this,o||{preventScroll:true});};' +
      'var I=WebAssembly.instantiate;WebAssembly.instantiate=function(m,i){' +
      'return I.call(WebAssembly,m,i).then(function(r){return r.instance?r:{module:m,instance:r};});};})();</script>\n';
    var doc = E.shell.replace(E.info.atlas_mark, function () { return m.atlas || ''; });
    var marker = '<canvas id="term"';
    doc = doc.replace(marker, function () { return scene + hook + marker; });
    doc = doc.replace('<title>WIBWOB-DOS</title>', function () { return '<title>' + m.title.replace(/</g, '&lt;') + '</title>'; });
    return doc.replace(E.info.glue_mark, function () { return E.glue; });
  }

  // ---- posters ----
  function sizeBox(p, m) {
    var w = m.size[0] * 8, h = (m.size[1] + 2) * 16;
    p.box.style.aspectRatio = w + ' / ' + h;
    p.box.style.width = 'min(100%, calc(85vh * ' + (w / h).toFixed(4) + '))';
    p.ratio = w / h; p.cols = m.size[0];
  }
  function fitPoster(p) {
    if (!p.pre || !p.cols) return;
    p.pre.style.fontSize = (p.box.clientWidth / p.cols / 0.6).toFixed(2) + 'px';
    p.pre.style.lineHeight = (p.box.clientHeight / (p.rows || 1)).toFixed(2) + 'px';
  }
  function showPoster(p) {
    manifest(p).then(function (m) {
      sizeBox(p, m);
      if (p.frame) return;
      p.box.innerHTML = '';
      var pre = document.createElement('pre');
      pre.className = 'wwdos-poster';
      pre.setAttribute('aria-label', m.title + ' (poster; click to play)');
      var rows = m.poster && m.poster.rows ? m.poster.rows : ['', '  ' + m.title, '', '  click to play'];
      pre.textContent = rows.join('\n');
      p.rows = rows.length; p.pre = pre;
      p.box.appendChild(pre);
      fitPoster(p);
    }).catch(function (e) { refuse(p, e); });
  }
  function refuse(p, e) {
    host.stats.refused.push(p.src + ': ' + e.message);
    p.box.textContent = p.src + ': ' + e.message;
    p.box.className += ' wwdos-refused';
    console.warn('film-host:', p.src, e.message);
  }

  // ---- mount / unmount ----
  function liveCount() { return host.live.length; }
  function mount(p) {
    if (p.frame || p.mounting) return;
    while (host.live.length >= cfg.max) {
      // the least recently seen player goes first; one out of view before one in view
      var v = host.live.slice().sort(function (a, b) { return (a.visible - b.visible) || (a.seen - b.seen); })[0];
      unmount(v, 'limit');
    }
    p.mounting = true;
    host.live.push(p);
    host.stats.maxLive = Math.max(host.stats.maxLive, liveCount());
    Promise.all([engine(), manifest(p)]).then(function (a) {
      var E = a[0], m = a[1];
      return sounds(p, m).then(function (snd) { return [E, m, snd]; });
    }).then(function (a) {
      if (!p.mounting) return;                       // unmounted while loading
      var E = a[0], m = a[1], snd = a[2];
      if (m.runtime.wasm !== E.info.runtime.wasm || m.runtime.shell !== E.info.runtime.shell)
        console.info('film-host: ' + p.src + ' was made for another engine build of the same ABI');
      p.opts = { autostart: cfg.autostart ? 1 : 0 };
      if (p.resumeAt > 0) p.opts.t = p.resumeAt;
      var f = document.createElement('iframe');
      f.title = m.title;
      f.className = 'wwdos-player';
      f.setAttribute('allow', 'fullscreen; autoplay');
      f.addEventListener('load', function () { watchContext(p); });
      f.srcdoc = playerDoc(E, m, snd, p.id);
      p.box.innerHTML = '';
      p.pre = null;
      p.box.appendChild(f);
      p.frame = f; p.mounting = false;
      host.stats.mounts++;
      log('mount ' + p.src + (p.opts.t ? ' at ' + p.opts.t.toFixed(2) : ''));
    }).catch(function (e) {
      p.mounting = false;
      var i = host.live.indexOf(p); if (i >= 0) host.live.splice(i, 1);
      refuse(p, e);
    });
  }
  function stageOf(p) { try { return p.frame && p.frame.contentWindow && p.frame.contentWindow.wwdosStage; } catch (e) { return null; } }
  function canvasOf(p) { try { return p.frame.contentDocument.getElementById('term'); } catch (e) { return null; } }
  // A context the browser takes away (not one this host releases): the shell's own handler would reload the page at
  // the same second, which an about:srcdoc frame cannot do; the host does the same thing by unmounting at that
  // second and mounting again while the film is in view.
  function watchContext(p) {
    var c = canvasOf(p);
    if (!c) return;
    c.addEventListener('webglcontextlost', function () {
      if (p.disposing) return;
      host.stats.lost++;
      log('context lost ' + p.src);
      unmount(p, 'context lost');
      if (p.visible) setTimeout(function () { if (p.visible) mount(p); }, 250);
    });
  }
  function unmount(p, why) {
    var i = host.live.indexOf(p);
    if (i >= 0) host.live.splice(i, 1);
    clearTimeout(p.idle);
    if (p.mounting) { p.mounting = false; log('cancel ' + p.src); return; }
    if (!p.frame) return;
    var S = stageOf(p);
    try { if (S && S.length() > 0) p.resumeAt = S.time(); } catch (e) {}
    p.disposing = true;
    try {
      var c = canvasOf(p), gl = c && c.getContext('webgl2'), x = gl && gl.getExtension('WEBGL_lose_context');
      if (S) S.pause(true);
      if (x) x.loseContext();                        // give the context back now, not at garbage collection
    } catch (e) {}
    p.frame.srcdoc = '';
    p.frame.remove();                                // the document, its wasm instance, memory and audio go with it
    p.frame = null; p.disposing = false;
    host.stats.unmounts++;
    log('unmount ' + p.src + ' (' + why + ') at ' + (p.resumeAt || 0).toFixed(2));
    showPoster(p);
  }

  // ---- visibility ----
  function onSeen(entries) {
    entries.forEach(function (en) {
      var p = host.players[en.target.dataset.wwdosId];
      if (!p) return;
      var vis = en.isIntersecting && en.intersectionRatio >= cfg.threshold;
      var gone = !en.isIntersecting || en.intersectionRatio === 0;
      if (vis) {
        p.visible = 1; p.seen = performance.now(); clearTimeout(p.idle);
        mount(p);
      } else if (gone) {
        p.visible = 0;
        if (p.frame || p.mounting) {
          clearTimeout(p.idle);
          p.idle = setTimeout(function () { if (!p.visible) unmount(p, 'out of view'); }, cfg.idleMs);
        }
      } else {
        p.visible = 0;   // partly in view: keeps playing, not mounted for this alone
      }
    });
  }
  function start() {
    var els = document.querySelectorAll('[data-film]');
    var io = new IntersectionObserver(onSeen, { threshold: [0, cfg.threshold, 1] });
    var near = new IntersectionObserver(function (ens) {
      ens.forEach(function (en) { if (en.isIntersecting) { showPoster(host.players[en.target.dataset.wwdosId]); near.unobserve(en.target); } });
    }, { rootMargin: '100% 0px' });
    Array.prototype.forEach.call(els, function (el) {
      var id = nextId++;
      el.dataset.wwdosId = id;
      var box = document.createElement('div');
      box.className = 'wwdos-film';
      el.appendChild(box);
      var p = host.players[id] = { id: id, el: el, box: box, src: el.dataset.film, frame: null, visible: 0, seen: 0, resumeAt: 0 };
      box.addEventListener('click', function () { p.seen = performance.now(); mount(p); });
      io.observe(el); near.observe(el);
    });
    window.addEventListener('resize', function () { Object.keys(host.players).forEach(function (k) { fitPoster(host.players[k]); }); });
    // File > Exit in a player hands back to the page: unmount it
    window.addEventListener('message', function (e) {
      if (!e.data || e.data.wwdos !== 'exit') return;
      Object.keys(host.players).forEach(function (k) {
        var p = host.players[k];
        if (p.frame && p.frame.contentWindow === e.source) { unmount(p, 'exit'); p.resumeAt = 0; }
      });
    });
  }
  host.mount = function (id) { mount(host.players[id]); };
  host.unmount = function (id) { unmount(host.players[id], 'asked'); };
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start); else start();
})();
