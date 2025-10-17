#!/usr/bin/env python3
import re, html, traceback, textwrap, json
from pathlib import Path
from typing import List, Dict, Any
from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
import httpx
import xml.etree.ElementTree as ET

# ============================================================
# Tiny template engine
# ============================================================

BASE_DIR = Path(__file__).parent
PY_DIR = BASE_DIR / "py"
DATA_DIR = BASE_DIR / "data"
PAYLOADS_DIR = DATA_DIR / "payloads"
STATIC_DIR = BASE_DIR / "static"
for d in (PY_DIR, DATA_DIR, PAYLOADS_DIR, STATIC_DIR):
    d.mkdir(parents=True, exist_ok=True)

RE_BLOCK = re.compile(r"{% block (\w+) %}(.*?){% endblock %}", re.S)
RE_EXTENDS = re.compile(r"{% extends ['\"](.*?)['\"] %}")
RE_INCLUDE = re.compile(r"{% include ['\"](.*?)['\"] %}")
RE_EXPR = re.compile(r"{{(.*?)}}", re.S)
RE_CODE = re.compile(r"<%(.*?)%>", re.S)

PREFERRED_ENCODINGS = ["utf-8", "utf-8-sig", "cp1250", "cp1252", "iso-8859-2", "latin-1"]

def _read_text_best_effort(path: Path) -> str:
    data = path.read_bytes()
    for enc in PREFERRED_ENCODINGS:
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            continue
    # last resort: replace errors so at least it renders
    return data.decode("utf-8", errors="replace")


def _render_template_source(src: str, file_dir: Path, request: Request,
                            *, form_data=None, allow_extends: bool):
    out_buf: List[str] = []
    ctx: Dict[str, Any] = {}

    def write(x): out_buf.append(html.escape(str(x)))
    def write_raw(x): out_buf.append(str(x))
    def now(): import datetime; return datetime.datetime.now()
    def params_get(key, default=None):
        return form_data.get(key) if form_data and key in form_data else request.query_params.get(key, default)
    def http_post_raw(url: str, payload: str, *, content_type="application/xml"):
        try:
            r = httpx.post(url, data=payload.encode("utf-8"), headers={"Content-Type": content_type}, timeout=10)
            return {"ok": True, "status": r.status_code, "body": r.text}
        except Exception as e:
            return {"ok": False, "error": str(e)}

    ctx.update(dict(
        request=request, form_data=form_data,
        write=write, write_raw=write_raw, now=now,
        params_get=params_get, http_post_raw=http_post_raw,
        html=html, json=json,
        Path=Path, ET=ET,
        BASE_DIR=BASE_DIR,
        DATA_DIR=DATA_DIR,
    ))

    m_ext = RE_EXTENDS.search(src) if allow_extends else None
    if m_ext:
        parent_file = m_ext.group(1)
        parent_path = file_dir / parent_file
        parent_src = _read_text_best_effort(parent_path)
        parent_src = _replace_includes(parent_src, parent_path.parent, ctx)
        child_blocks = {m.group(1): m.group(2) for m in RE_BLOCK.finditer(src)}
 
        def _render_with_blocks(src2: str, dir2: Path, ctx2: Dict[str, Any], overrides: Dict[str,str]):
          pos = 0
          for m in RE_BLOCK.finditer(src2):
            # EXECUTE the text before the block (so {{ ... }} and <% ... %> run)
            _exec_template(src2[pos:m.start()], dir2, ctx2)

            name, body = m.group(1), m.group(2)
            chosen = overrides.get(name, body)
            _exec_template(chosen, dir2, ctx2)

            pos = m.end()
          # EXECUTE the trailing text after the last block
          _exec_template(src2[pos:], dir2, ctx2)

        _render_with_blocks(parent_src, parent_path.parent, ctx, overrides=child_blocks)
        return "".join(out_buf)

    src = _replace_includes(src, file_dir, ctx)
    _exec_template(src, file_dir, ctx)
    return "".join(out_buf)

def _replace_includes(src: str, file_dir: Path, ctx: Dict[str,Any]):
    def repl(m):
        fn = m.group(1)
        path = file_dir / fn
        try:
            inner = _read_text_best_effort(path)
            return _render_template_source(inner, path.parent, ctx["request"],
                                           form_data=ctx["form_data"], allow_extends=False)
        except Exception as e:
            return f"<!-- include error {fn}: {e} -->"
    return RE_INCLUDE.sub(repl, src)

def _exec_template(src: str, file_dir: Path, ctx: Dict[str,Any]):
    pos = 0
    for m in RE_CODE.finditer(src):
        text = src[pos:m.start()]
        _exec_text_with_exprs(text, ctx)
        code = textwrap.dedent(m.group(1)).strip("\n")
        try:
            exec(code, ctx, ctx)
        except Exception:
            ctx["write_raw"](f"<pre>Template code error inside &lt;% ... %&gt;:\n{html.escape(code)}\n---\n{html.escape(traceback.format_exc())}</pre>")
        pos = m.end()
    _exec_text_with_exprs(src[pos:], ctx)

def _apply_simple_filters(val, filters):
    """Apply simple Jinja-like no-arg filters in order."""
    for f in filters:
        name = f.strip().lower()
        if not name:
            continue
        if name in ("safe",):           # no-op; output stays unescaped
            pass
        elif name in ("escape", "e"):   # HTML-escape
            val = html.escape(str(val))
        elif name == "upper":
            val = str(val).upper()
        elif name == "lower":
            val = str(val).lower()
        elif name == "title":
            val = str(val).title()
        elif name in ("trim", "strip"):
            val = str(val).strip()
        elif name == "json":
            val = json.dumps(val, ensure_ascii=False)
        elif name == "length":
            try:
                val = len(val)
            except Exception:
                val = 0
        else:
            # Unknown filter -> ignore quietly
            pass
    return val


def _exec_text_with_exprs(text: str, ctx: Dict[str,Any]):
    pos = 0
    for m in RE_EXPR.finditer(text):
        ctx["write_raw"](text[pos:m.start()])
        raw = m.group(1)
        expr = raw.strip()
        try:
            # Support simple Jinja-like pipes: expr|filter1|filter2
            parts = [p.strip() for p in expr.split("|")]
            base_expr = parts[0]
            filters = parts[1:] if len(parts) > 1 else []

            val = eval(base_expr, ctx, ctx)
            if filters:
                val = _apply_simple_filters(val, filters)

            ctx["write_raw"](str(val))
        except Exception:
            ctx["write_raw"](
                f"<pre>Template expression error in: {{ {html.escape(raw)} }}\n"
                f"{html.escape(traceback.format_exc())}</pre>"
            )
        pos = m.end()
    ctx["write_raw"](text[pos:])

def render_template(filename: str, request: Request, form_data=None):
    path = PY_DIR / filename
    src = _read_text_best_effort(path)
    return _render_template_source(src, path.parent, request,
                                   form_data=form_data, allow_extends=True)

# ============================================================
# FastAPI app
# ============================================================

app = FastAPI()
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    if (PY_DIR / "index.pyhtml").exists():
        return render_template("index.pyhtml", request)
    else:
        links = [p.name for p in PY_DIR.glob("*.pyhtml")]
        body = "<h1>Available pages</h1><ul>" + "".join(
            f"<li><a href='/py/{name[:-7]}'>{name}</a></li>" for name in links
        ) + "</ul>"
        return HTMLResponse(body)

@app.api_route("/py/{name}", methods=["GET", "POST"], response_class=HTMLResponse)
async def serve_py(name: str, request: Request):
    file = f"{name}.pyhtml"
    path = PY_DIR / file
    if not path.exists():
        return HTMLResponse(f"<h1>Not found: {file}</h1>", status_code=404)
    form_data = {}
    if request.method == "POST":
        form = await request.form()
        form_data = dict(form)
        if "use_prg" in form_data:
            url = str(request.url).split("?")[0]
            return RedirectResponse(url=url, status_code=303)
    return render_template(file, request, form_data=form_data)

# ============================================================
# Seed templates
# ============================================================

(PY_DIR / "base.pyhtml").write_text("""<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>{% block title %}Demo{% endblock %}</title>
  <link rel="stylesheet" href="/static/site.css">
</head>
<body>
  <nav><a href="/">Home</a> | <a href="/py/hello?name=World">Hello</a> | <a href="/py/form_no_prg">Form (no PRG)</a> | <a href="/py/form_prg">Form (with PRG)</a> | <a href="/py/entities">Entities</a></nav>
  <hr>
  {% block content %}{% endblock %}
  <hr>
  <footer><small>&copy; {{ now().year }}</small></footer>
</body>
</html>
""", encoding="utf-8")

(PY_DIR / "hello.pyhtml").write_text("""{% extends 'base.pyhtml' %}
{% block title %}Hello Page{% endblock %}
{% block content %}
<h1>Hello, {{ params_get('name') or 'World' }}</h1>
<ul>
<%
for i in range(3):
    write("<li>Item %d</li>" % i)
%>
</ul>
<p>Now: {{ now() }}</p>
{% endblock %}
""", encoding="utf-8")

(PY_DIR / "form_no_prg.pyhtml").write_text("""{% extends 'base.pyhtml' %}
{% block content %}
<h1>Form without PRG</h1>
<form method="post">
  <label>Name: <input name="name" value="{{ params_get('name','') }}"></label>
  <button>Submit</button>
</form>
<%
if params_get('name'):
    write("<p>You submitted: " + params_get('name') + "</p>")
%>
{% endblock %}
""", encoding="utf-8")

(PY_DIR / "form_prg.pyhtml").write_text("""{% extends 'base.pyhtml' %}
{% block content %}
<h1>Form with PRG</h1>
<form method="post">
  <input type="hidden" name="use_prg" value="1">
  <label>Name: <input name="name" value="{{ params_get('name','') }}"></label>
  <button>Submit</button>
</form>
<%
if params_get('name'):
    write("<p>You submitted (PRG): " + params_get('name') + "</p>")
%>
{% endblock %}
""", encoding="utf-8")

(PY_DIR / "entities.pyhtml").write_text(r'''{% extends 'base.pyhtml' %}
{% block title %}O1 demo{% endblock %}
{% block content %}

<style>
  #payloadRadios { display:flex; flex-wrap:wrap; gap:0.5rem 1rem; }
  #payloadRadios label { display:inline-flex; align-items:center; gap:0.4rem; cursor:pointer; }

  .split { display:grid; grid-template-columns: 1fr 1fr; gap: 1rem; align-items:start; }
  @media (max-width: 900px) { .split { grid-template-columns: 1fr; } }

  .result-box {
    background:#fafafa; border:1px solid #ddd; border-radius:8px;
    padding:0.75rem; overflow:auto; max-height:60vh; white-space:pre-wrap;
    font-family: monospace; font-size: 0.9em;
  }
  .muted { opacity:0.7; }
  .error { color:#b00020; }
/* Energy labels + bullet-proof tooltips */
.labels-wrap { display:flex; flex-wrap:wrap; gap:0.25rem 0.4rem; margin-top:0.5rem; }
.label {
  position: relative;
  display:inline-block; padding:0.2rem 0.6rem; border-radius:9999px;
  border:1px solid #ddd; font-size:0.85em; line-height:1.2;
}
.label-green  { background:#e8f5e9; border-color:#c8e6c9; color:#2e7d32; }
.label-yellow { background:#fff8e1; border-color:#ffe082; color:#b28704; }
.label-gray   { background:#f3f4f6; border-color:#e5e7eb; color:#374151; }

/* Inline tooltip (no attr(...)) */
.label .tip {
  position: absolute;
  left: 50%; transform: translateX(-50%);
  bottom: 100%; margin-bottom: 6px;
  background: #111; color: #fff;
  padding: 4px 8px; border-radius: 4px;
  font-size: 0.8em; white-space: nowrap;
  opacity: 0; pointer-events: none; transition: opacity .12s;
  z-index: 1000;
}
.label .tip::after {
  content: "";
  position: absolute;
  left: 50%; transform: translateX(-50%);
  top: 100%;
  border: 6px solid transparent;
  border-top-color: #111;
}
.label:hover .tip,
.label:focus .tip { opacity: 1; }

</style>

<h1>O1 demo</h1>
<%
import xml.etree.ElementTree as ET
from pathlib import Path

# --- Load entities from <config><entry>... ---
def _lname(tag: str) -> str:
    # '{ns}Tag' -> 'tag', or 'Tag' -> 'tag'
    return tag.split('}', 1)[-1].lower() if '}' in tag else tag.lower()
def _txt(s): return (s or '').strip()

# Accept common synonyms (all compared lower-cased)
NAME_TAGS = {'name', 'cell_name', 'cellname'} 
GNB_TAGS  = {'gnodeb_id','gnodebid','gnb_id','gnbid','gnb','gnodeb'}
CELL_TAGS = {'relativecellid','relative_cell_id','cell_id','cellid','id'}

entities = []
try:
    # Pick the first existing file from these common locations
    ent_paths = [
	Path('../config_data/xml/Topo_Osiris_5G_2025-02-12_100122.xml'),
        Path('data/entities.xml'),
        Path('data/Entities.xml'),
        Path('data/results/entities.xml'),
        Path('data/results/Entities.xml'),
    ]
    ent_file = next((p for p in ent_paths if p.exists()), None)
    if not ent_file:
        write_raw("<p class='error'>No entities XML found in data/ or data/results/.</p>")
    else:
        # tolerant read if you have it
        try:
            xml_txt = read_text_best(ent_file)   # your tolerant reader
        except Exception:
            xml_txt = ent_file.read_text(encoding='utf-8')

        root = ET.fromstring(xml_txt)

        # Look specifically for <entry> nodes anywhere (e.g., under <config>)
        found = []
        for entry in root.iter():
            if _lname(entry.tag) != 'entry':
                continue

            rec = {'name':'', 'gnodeb_id':'', 'cell_id':''}
            for ch in list(entry):
                tag = _lname(ch.tag)
                val = _txt(ch.text)
                if not val:
                    continue
                if tag in NAME_TAGS:
                    rec['name'] = val
                elif tag in GNB_TAGS:
                    rec['gnodeb_id'] = val
                elif tag in CELL_TAGS:
                    rec['cell_id'] = val

            # Require at least a name + one of the IDs
            if rec['name'] and (rec['gnodeb_id'] or rec['cell_id']):
                found.append(rec)

        # De-duplicate while preserving order
        seen, entities = set(), []
        for e in found:
            key = (e['name'], e.get('gnodeb_id',''), e.get('cell_id',''))
            if key not in seen:
                entities.append(e); seen.add(key)

        if not entities:
            write_raw("<p class='error'>Parsed 0 entities from "
                      + html.escape(str(ent_file)) +
                      ". Expected tags &lt;Name&gt;, &lt;gnodeb_id&gt;, &lt;RelativeCellId&gt; inside &lt;entry&gt;.</p>")
except Exception as e:
    write_raw("<p class='error'>Failed to read entities XML: " + html.escape(str(e)) + "</p>")


# Best-effort read (payloads may not be UTF-8)
def read_text_best(p):
    data = p.read_bytes()
    for enc in ("utf-8","utf-8-sig","cp1250","cp1252","iso-8859-2","latin-1"):
        try: return data.decode(enc)
        except Exception: pass
    return data.decode("utf-8","replace")

# Load payloads (alphabetical)
payload_dir = Path('data/payloads'); payloads=[]
if payload_dir.exists():
    for p in sorted(payload_dir.glob('*.xml'), key=lambda x: x.name.lower()):
        try: txt = read_text_best(p)
        except Exception as e: txt = f"<!-- failed {p.name}: {e} -->"
        payloads.append({"name": p.stem, "filename": p.name, "content": txt})


# --- Load energySavingState.xml (robust to namespaces, case, spaces) ---
def _localname(tag: str) -> str:
    # '{ns}Tag' -> 'tag', or 'Tag' -> 'tag'
    return tag.split('}', 1)[-1].lower() if '}' in tag else tag.lower()

energy_items = []
try:
    es_file = Path('data/results/energySavingState.xml')
    if es_file.exists():
        # tolerant read if you have it; else UTF-8
        try:
            txt = read_text_best(es_file)
        except Exception:
            txt = es_file.read_text(encoding='utf-8')

        root = ET.fromstring(txt)

        for entry in root.iter():
            if _localname(entry.tag) != 'entry':
                continue

            nm, st = '', ''
            for child in list(entry):
                lname = _localname(child.tag)
                if lname in ('cell_name', 'cellname'):
                    nm = (child.text or '').strip()
                elif lname == 'name' and not nm:
                    nm = (child.text or '').strip()
                elif lname == 'energysavingstate':
                    st = (child.text or '').strip()
            if not nm:
                continue

            # Normalize for classification: keep letters only
            s_norm = ''.join(c for c in st.lower() if c.isalpha())
            if 'energysaving' in s_norm:
                css = 'label-yellow' if 'not' in s_norm else 'label-green'
            else:
                css = 'label-gray'

            energy_items.append({'name': nm, 'state': st, 'css': css})
    # else: no file -> leave energy_items empty
except Exception as e:
    write_raw("<p class='error'>Failed to read energySavingState.xml: " + html.escape(str(e)) + "</p>")

#endpoint = params_get('endpoint', 'https://httpbin.org/post')
endpoint = params_get('endpoint', 'http://localhost:8831')
payload = params_get('payload', '')
sel_name = params_get('name','')
sel_payload_file = params_get('payload_file','')
%>

<form method="post" id="rpcForm">
  <div style="display:grid;gap:0.75rem">

    <label>
      Endpoint:
      <input name="endpoint" value="{{ endpoint|safe }}" style="width:100%">
    </label>

    <label>
      Entity:
      <div style="display:flex; gap:0.5rem; align-items:center; margin-bottom:0.35rem;">
        <input id="entityFilter" type="text" placeholder="Filter by Name / gNodeB / Cell..." style="flex:1;">
        <small id="entityCount" class="muted"></small>
      </div>

      <!-- Server-side fallback so list is NEVER empty -->
      <select id="nameSelect" name="name" style="width:100%">
      <%
      for e in entities:
          label = f"{e['name']} -> {e.get('gnodeb_id','')} -> {e.get('cell_id','')}"
          write_raw(
            "<option value='"+html.escape(e['name'])+"' " +
            "data-gnb='"+html.escape(e.get('gnodeb_id',''))+"' " +
            "data-cell='"+html.escape(e.get('cell_id',''))+"'>" +
            html.escape(label) + "</option>"
          )
      %>
      </select>
    </label>

    <fieldset style="border:1px solid #ddd;padding:0.75rem;border-radius:8px;">
      <legend style="padding:0 0.25rem;">Payload (alphabetical)</legend>
      <div id="payloadRadios">
      <%
      if not payloads:
          write_raw("<em>No payloads found in data/payloads/</em>")
      else:
          for i, p in enumerate(payloads):
              is_checked = (p["filename"] == sel_payload_file) or (i == 0 and not sel_payload_file)
              checked = " checked" if is_checked else ""
              rid = "pl_" + str(i)
              write_raw(
                f"<label for='{rid}'>"
                f"<input type='radio' name='payload_file' value='{html.escape(p['filename'])}' id='{rid}'{checked}>"
                f"{html.escape(p['name'])}"
                f"</label>"
              )
      %>
      </div>
    </fieldset>

    <div class="split">
      <div>
        <div style="display:flex;gap:0.5rem;flex-wrap:wrap;margin-bottom:0.5rem">
          <button type="button" id="btnInjectId">Insert entity ID</button>
          <button type="button" id="btnMsgId">Insert MESSAGE_ID</button>
          <small id="helperText" class="muted"></small>
        </div>

        <label>
          Payload:
          <textarea id="payload" name="payload" rows="18" style="width:100%;font-family:monospace">{{ payload|safe }}</textarea>
        </label>

        <div style="margin-top:0.5rem">
          <button>Send XML POST</button>
        </div>
      </div>

      <div>
<%
if params_get('payload'):  # only show after a send
    sel_fn = params_get('payload_file','')
    display_name = ''
    for p in payloads:
        if p['filename'] == sel_fn:
            display_name = p['name']; break
    if display_name or sel_fn:
        dn = display_name or sel_fn
        write_raw("<div class='muted' style='margin-bottom:0.5rem'><b>Sent payload:</b> " + html.escape(dn) + "</div>")
%>

        <h3 style="margin-top:0">Result</h3>
        <div class="result-box">
<%
import re as _re, datetime as _dt
ep = params_get('endpoint')
payload_raw = params_get('payload')

if ep and payload_raw:
    # 1) Detect first-line directive: <!-- SAVE: filename.ext -->
    m = _re.match(r'^\s*<!--\s*SAVE\s*:\s*([a-zA-Z0-9._-]+)\s*-->\s*(?:\r?\n)?', payload_raw)
    save_as = None
    if m:
        save_as = m.group(1)
        payload_to_send = payload_raw[m.end():]   # strip directive from sent body
    else:
        payload_to_send = payload_raw

    # 2) Send HTTP POST with stripped payload
    r = http_post_raw(ep, payload_to_send)

    if r.get("ok"):
        body = r.get("body","")
        write_raw("Status: "+str(r.get('status'))+"\n\n")
        write_raw(html.escape(body))

        # 3) Save result into data/results/<filename>
        try:
            resdir = DATA_DIR / "results"
            resdir.mkdir(parents=True, exist_ok=True)
            ts = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
            if not save_as:
                base = params_get('payload_file','') or 'result'
                base = Path(base).stem or 'result'
                save_as = f"{base}-{ts}.txt"
            # sanitize filename
            save_as = _re.sub(r'[^a-zA-Z0-9._-]', '_', save_as)
            (resdir / save_as).write_text(body, encoding="utf-8")
            write_raw("\n\nSaved to: data/results/" + html.escape(save_as))
        except Exception as e:
            write_raw("\n\nSave error: " + html.escape(str(e)))
    else:
        write_raw("Error: "+html.escape(r.get("error","")))
else:
    write_raw("Result will appear here after you click Send.")
%>

        </div>
      </div>
    </div>

  </div>
</form>

<div style="margin-top:1rem">
  <div style="display:flex; align-items:center; gap:0.5rem; margin-bottom:0.4rem;">
    <h4 style="margin:0">Energy Saving State</h4>
    <button type="button" id="btnReloadEnergy">Read Energy State</button>
  </div>
  <div class="labels-wrap" id="energyLabels">
  <%
  for it in energy_items:
      nm = it.get('name') or ''
      st = it.get('state') or 'unknown'
      css = it.get('css') or 'label-gray'
      write_raw(
          "<span class='label " + css + "' tabindex='0' title='" + html.escape(st) + "'>"
          + html.escape(nm)
          + "<span class='tip'>" + html.escape(st) + "</span>"
          + "</span>"
      )
  %>
  </div>
</div>

<!-- Safe JSON blobs for JS (no inline JS parsing issues) -->
<script type="application/json" id="entitiesData">{{ json.dumps(entities)|safe }}</script>
<script type="application/json" id="payloadsData">{{ json.dumps(payloads)|safe }}</script>
<script type="application/json" id="defaultEntity">{{ json.dumps(sel_name)|safe }}</script>

<script>
document.addEventListener('DOMContentLoaded', () => {
  const err = (msg, e) => { console.error(msg, e||''); const hc=document.getElementById('helperText'); if(hc){hc.textContent=msg; hc.classList.add('error');} };

  try {
    // Read data
    const ENTITIES = JSON.parse(document.getElementById('entitiesData')?.textContent || '[]');
    const PAYLOADS = JSON.parse(document.getElementById('payloadsData')?.textContent || '[]');
    const DEFAULT_ENTITY = JSON.parse(document.getElementById('defaultEntity')?.textContent || '""');

    // Elements
    const nameSel      = document.getElementById('nameSelect');
    const entityFilter = document.getElementById('entityFilter');
    const entityCount  = document.getElementById('entityCount');
    const payloadTA    = document.getElementById('payload');
    const btnInjectId  = document.getElementById('btnInjectId');
    const btnMsgId     = document.getElementById('btnMsgId');
    const helper       = document.getElementById('helperText');

    if (!nameSel) return err('Init failed: #nameSelect not found');

    const setHelper=(m)=>{ if(helper){ helper.textContent=m||''; helper.classList.remove('error'); } };
    const escapeHtml=(s)=>String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');
    const entityByName=(n)=>ENTITIES.find(x=>x.name===n);
    const payloadByFile=(fn)=>PAYLOADS.find(x=>x.filename===fn);

  const btnReloadEnergy = document.getElementById('btnReloadEnergy');
  const energyLabels    = document.getElementById('energyLabels');

// Replace GNODEB_ID / CELL_ID only if we actually have values

// --- Robust, engine-safe placeholder injection ---

// inside DOMContentLoaded(...)
const OPEN  = '{' + '{';      // build tokens at runtime; no literal '{{' in source
const CLOSE = '}' + '}';

function getSelectedEntityValues(){
  const sel = document.getElementById('nameSelect');
  const opt = sel && sel.selectedOptions ? sel.selectedOptions[0] : null;
  let gnb  = opt ? (opt.dataset?.gnb  ?? opt.getAttribute('data-gnb')  ?? '') : '';
  let cell = opt ? (opt.dataset?.cell ?? opt.getAttribute('data-cell') ?? '') : '';
  if ((!gnb || !cell) && opt) {
    const parts = (opt.textContent || '').split('->').map(s => s.trim());
    if (!gnb  && parts.length >= 2) gnb  = parts[1];
    if (!cell && parts.length >= 3) cell = parts[2];
  }
  return { gnb, cell };
}

function injectEntityPlaceholders(payload, gnb, cell){
  if (!payload) return payload;

  const reGnb  = new RegExp(OPEN + '\\s*GNODEB_ID\\s*' + CLOSE, 'gi');
  const reCell = new RegExp(OPEN + '\\s*CELL_ID\\s*'   + CLOSE, 'gi');

  let out = payload;
  if (gnb  != null && String(gnb).trim()  !== '') out = out.replace(reGnb,  String(gnb).trim());
  if (cell != null && String(cell).trim() !== '') out = out.replace(reCell, String(cell).trim());
  return out;
}

document.getElementById('btnInjectId')?.addEventListener('click', ()=>{
  const ta  = document.getElementById('payload');
  const sel = document.getElementById('nameSelect');
  if (!ta || !sel || !sel.selectedOptions.length) return;

  const opt  = sel.selectedOptions[0];
  const gnb  = opt.dataset?.gnb  ?? opt.getAttribute('data-gnb')  ?? '';
  const cell = opt.dataset?.cell ?? opt.getAttribute('data-cell') ?? '';

  const before = ta.value;
  const after  = injectEntityPlaceholders(before, gnb, cell);
  //console.log(after)
  ta.value = after;

  // feedback (no curly braces in regex literals)
  const hadGnb  = new RegExp(OPEN + '\\s*GNODEB_ID\\s*' + CLOSE, 'i').test(before);
  const hadCell = new RegExp(OPEN + '\\s*CELL_ID\\s*'   + CLOSE, 'i').test(before);
  const msgs = [];
  if (hadGnb)  msgs.push(gnb  ? `GNODEB_ID=${gnb}` : 'GNODEB_ID missing');
  if (hadCell) msgs.push(cell ? `CELL_ID=${cell}`  : 'CELL_ID missing');
  (document.getElementById('helperText')||{}).textContent =
    msgs.length ? 'Filled: ' + msgs.join(', ') : 'No placeholders found.';
});

  function renderEnergyLabels(items){
    if (!energyLabels) return;
    const esc = s => String(s)
      .replace(/&/g,'&amp;').replace(/</g,'&lt;')
      .replace(/>/g,'&gt;').replace(/"/g,'&quot;').replace(/'/g,'&#39;');
    energyLabels.innerHTML = (items || []).map(it => {
      const nm  = esc(it.name || '');
      const st  = esc(it.state || 'unknown');
      const css = esc(it.css || 'label-gray');
      return (
        `<span class="label ${css}" tabindex="0" title="${st}">` +
          `${nm}<span class="tip">${st}</span>` +
        `</span>`
      );
    }).join('');
  }

  if (btnReloadEnergy) {
    btnReloadEnergy.addEventListener('click', async () => {
      try {
        const res = await fetch('/api/energy_state?ts=' + Date.now(), {cache:'no-store'});
        const data = await res.json();
        if (data && data.ok) {
          renderEnergyLabels(data.items);
          setHelper && setHelper('Energy state reloaded.');
        } else {
          setHelper && setHelper('Failed to read energy state' + (data?.error ? ': ' + data.error : ''));
        }
      } catch (e) {
        setHelper && setHelper('Error reading energy state: ' + e);
        console.error(e);
      }
    });
  }


    // --- Filterable options (rebuild from ENTITIES, but we start from server-rendered options) ---
function renderEntityOptions(filterText){
  const prev = nameSel.value || DEFAULT_ENTITY || "";
  const f = (filterText || "").trim().toLowerCase();

  const list = ENTITIES.filter(e => {
    const a = (e.name||"").toLowerCase();
    const b = (e.gnodeb_id||"").toLowerCase();
    const c = (e.cell_id||"").toLowerCase();
    return !f || a.includes(f) || b.includes(f) || c.includes(f);
  });

  nameSel.innerHTML = list.map(e => {
    const label = `${e.name||''} -> ${e.gnodeb_id||''} -> ${e.cell_id||''}`;
    return `<option value="${escapeHtml(e.name||'')}"
            data-gnb="${escapeHtml(e.gnodeb_id||'')}"
            data-cell="${escapeHtml(e.cell_id||'')}">
            ${escapeHtml(label)}</option>`;
  }).join("");

  if (list.some(e => e.name === prev)) nameSel.value = prev;
  else if (list.length) nameSel.value = list[0].name;

  if (entityCount) entityCount.textContent = `${list.length}/${ENTITIES.length}`;
}

    // Initial counter (in case you don't type)
    if (entityCount) entityCount.textContent = `${nameSel.options.length}/${ENTITIES.length}`;

    // Live filter
    if (entityFilter){
      let t=null;
      entityFilter.addEventListener('input', ()=>{
        if (t) clearTimeout(t);
        t = setTimeout(()=> renderEntityOptions(entityFilter.value), 120);
      });
    }

    // --- Collapsible select: 1 row normally, 8 expanded ---
    const COLLAPSED_SIZE = 1, EXPANDED_SIZE = 8;
    const collapseSelect=()=>{ nameSel.size = COLLAPSED_SIZE; nameSel.dataset.expanded='0'; };
    const expandSelect=()=>{ nameSel.size = EXPANDED_SIZE;  nameSel.dataset.expanded='1'; };
    collapseSelect();

    nameSel.addEventListener('focus', expandSelect);
    nameSel.addEventListener('mousedown', (e) => {
      if (nameSel.size === COLLAPSED_SIZE) { e.preventDefault(); expandSelect(); nameSel.focus(); }
    });
    nameSel.addEventListener('change', () => { collapseSelect(); nameSel.blur(); });
    nameSel.addEventListener('keydown', (e) => {
      if (e.key==='Escape'||e.key==='Enter'||e.key==='Tab'){ collapseSelect(); nameSel.blur(); }
    });
    const entityWrap = nameSel.closest('label') || nameSel.parentElement || nameSel;
    document.addEventListener('pointerdown', (e) => {
      if (nameSel.dataset.expanded==='1' && !entityWrap.contains(e.target)) collapseSelect();
    }, true);
    if (entityFilter) {
      entityFilter.addEventListener('focus', expandSelect);
      entityFilter.addEventListener('input', () => expandSelect());
    }

    // --- Payload radios -> load content ---
    function currentRadio(){ return document.querySelector('input[name="payload_file"]:checked'); }
    const first = currentRadio();
    if (first && payloadTA && !payloadTA.value.trim()){
      const p = payloadByFile(first.value);
      if (p){ payloadTA.value = p.content; setHelper('Auto-loaded: ' + first.value); }
    }
    document.addEventListener('change', (ev)=>{
      if (ev.target && ev.target.name === 'payload_file') {
        const p = payloadByFile(ev.target.value);
        if (p && payloadTA) { payloadTA.value = p.content; setHelper('Loaded: ' + ev.target.value); }
      }
    });

    // --- Insert helpers (CELL_ID + <id>... </id>) ---
    let lastInjectedEntityId = null;
    let usedCellIdPlaceholder = false;

    function injectEntityId(xml, id){
      if (!xml) return xml;
      const rePH = /\{\{\s*CELL_ID\s*\}\}/g;
      if (rePH.test(xml)) {
        usedCellIdPlaceholder = true;
        lastInjectedEntityId = id;
        return xml.replace(rePH, id);
      }
      const reTag = /(<\s*id\s*>)[\s\S]*?(<\s*\/\s*id\s*>)/i;
      if (reTag.test(xml)) return xml.replace(reTag, `$1${id}$2`);
      return xml;
    }
    function injectMessageId(xml, msgId){
      if (!xml) return xml;
      if (/\{\{\s*MESSAGE_ID\s*\}\}/.test(xml)) return xml.replace(/\{\{\s*MESSAGE_ID\s*\}\}/g, msgId);
      if (/<\s*rpc\b[^>]*\bmessage-id\s*=/.test(xml)) return xml.replace(/(<\s*rpc\b[^>]*\bmessage-id\s*=\s*")[^"]*(")/i, `$1${msgId}$2`);
      return xml.replace(/<\s*rpc\b/i, `<rpc message-id="${msgId}"`);
    }
    const escapeRegExp=(s)=>String(s).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    /*
    if (btnInjectId) btnInjectId.addEventListener('click', ()=>{
      if (!payloadTA) return;
      const e = entityByName(nameSel.value);
      if (!e){ setHelper('Pick an entity first.'); return; }
      const before = payloadTA.value;
      const after  = injectEntityId(before, e.id);
      payloadTA.value = after;
      lastInjectedEntityId = e.id;

      if (/\{\{\s*CELL_ID\s*\}\}/.test(before)) setHelper('CELL_ID placeholder filled.');
      else if (/<\s*id\s*>/i.test(before)) setHelper('Entity ID inserted into <id>...</id>.');
      else setHelper('No CELL_ID placeholder or <id> element found.');
    });
    */
    if (btnMsgId) btnMsgId.addEventListener('click', ()=>{
      if (!payloadTA) return;
      const msgId = String(Date.now());
      payloadTA.value = injectMessageId(payloadTA.value, msgId);
      setHelper('MESSAGE_ID set: ' + msgId);
    });

    // Auto-update on entity change
    nameSel.addEventListener('change', ()=>{
      if (!payloadTA) return;
      const e = entityByName(nameSel.value); if (!e) return;

      const hasIdTag = /<\s*id\s*>[\s\S]*?<\s*\/\s*id\s*>/i.test(payloadTA.value);
      if (hasIdTag){
        //payloadTA.value = payloadTA.value.replace(/(<\s*id\s*>)[\s\S]*?(<\s*\/\s*id\s*>)/i, `$1${e.id}$2`);
        //setHelper('Entity changed -> <id> updated.');
        return;
      }
      if (usedCellIdPlaceholder && lastInjectedEntityId){
        const reOld = new RegExp(escapeRegExp(String(lastInjectedEntityId)), 'g');
        if (reOld.test(payloadTA.value)) {
          //payloadTA.value = payloadTA.value.replace(reOld, String(e.id));
          //setHelper('Entity changed -> CELL_ID value updated.');
          //lastInjectedEntityId = e.id;
        }
      }
    }); 

  } catch (e) {
    err('Init error - check console for details.', e);
  }
});
</script>

{% endblock %} 
''', encoding="utf-8")

# ============================================================
# Seed data (entities + payloads)
# ============================================================

entities_xml = """<entities>
  <entity><name>name1</name><id>2625684</id></entity>
  <entity><name>name2</name><id>9876543</id></entity>
  <entity><name>name3</name><id>1234567</id></entity>
</entities>
"""
if not (DATA_DIR/"entities.xml").exists():
    (DATA_DIR/"entities.xml").write_text(entities_xml, encoding="utf-8")

turnoff_xml = """<?xml version="1.0"?>
<rpc xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="{{MESSAGE_ID}}">
  <edit-config><target><running/></target><config>
    <ManagedElement xmlns="urn:3gpp:sa5:_3gpp-common-managed-element"><id></id></ManagedElement>
  </config></edit-config>
</rpc>
"""
if not (PAYLOADS_DIR/"TurnOFF.xml").exists():
    (PAYLOADS_DIR/"TurnOFF.xml").write_text(turnoff_xml, encoding="utf-8")

turnon_xml = """<?xml version="1.0"?>
<rpc xmlns="urn:ietf:params:xml:ns:netconf:base:1.0" message-id="{{MESSAGE_ID}}">
  <edit-config><target><running/></target><config>
    <ManagedElement xmlns="urn:3gpp:sa5:_3gpp-common-managed-element"><id></id></ManagedElement>
  </config></edit-config>
</rpc>
"""
if not (PAYLOADS_DIR/"TurnOn.xml").exists():
    (PAYLOADS_DIR/"TurnOn.xml").write_text(turnon_xml, encoding="utf-8")

# ============================================================
# Seed static CSS
# ============================================================

(STATIC_DIR/"site.css").write_text("body{font-family:sans-serif;margin:1em;}nav a{margin-right:1em;}textarea{font-size:0.9em;}", encoding="utf-8")

# --- API: read data/results/energySavingState.xml and return labels as JSON ---
def _localname(tag: str) -> str:
    return tag.split('}', 1)[-1].lower() if '}' in tag else tag.lower()

def _classify_energy_state(raw: str) -> str:
    s = ''.join(c for c in (raw or '').lower() if c.isalpha())
    if 'energysaving' in s:
        return 'label-yellow' if 'not' in s else 'label-green'
    return 'label-gray'

def _parse_energy_items_from_text(txt: str):
    root = ET.fromstring(txt)
    items = []
    for entry in root.iter():
        if _localname(entry.tag) != 'entry':
            continue
        nm, st = '', ''
        for child in list(entry):
            lname = _localname(child.tag)
            val = (child.text or '').strip()
            if lname in ('cell_name', 'cellname'):
                nm = val
            elif lname == 'name' and not nm:   # fallback if no <cell_name>
                nm = val
            elif lname == 'energysavingstate':
                st = val
        if nm:
            items.append({
                "name": nm,
                "state": st,
                "css": _classify_energy_state(st),
            })
    return items

# --- API: (re)read data/results/EnergySavingState.xml on demand ---
@app.get("/api/energy_state")
async def api_energy_state():
    path = DATA_DIR / "results" / "EnergySavingState.xml"
    if not path.exists():
        return {"ok": False, "items": [], "error": "data/results/EnergySavingState.xml not found"}
    try:
        try:
            # use your tolerant reader if present
            txt = _read_text_best_effort(path)   # falls back below if not defined
        except Exception:
            txt = path.read_text(encoding="utf-8")
        items = _parse_energy_items_from_text(txt)
        return {"ok": True, "items": items}
    except Exception as e:
        return {"ok": False, "items": [], "error": str(e)}

if __name__ == "__main__":
    import os, uvicorn
    port = int(os.environ.get("PORT", "8001"))
    # Using the import string lets --reload work even when you run the file directly
    uvicorn.run("O1_demo:app", host="0.0.0.0", port=port, reload=True, log_level="info")

