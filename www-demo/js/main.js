/* webserv — shared JS
 * Handles: mobile nav, active link highlight, form preview, DELETE demo,
 * and live uptime counter on the homepage.
 */
(function () {
  // Mobile nav toggle
  document.addEventListener("click", function (e) {
    var t = e.target.closest("[data-nav-toggle]");
    if (t) {
      var links = document.querySelector(".nav-links");
      if (links) links.classList.toggle("open");
    }
  });

  // Active link highlight
  var path = window.location.pathname.replace(/\/$/, "") || "/index.html";
  if (path === "/" || path === "") path = "/index.html";
  document.querySelectorAll(".nav-links a").forEach(function (a) {
    var href = a.getAttribute("href");
    if (!href) return;
    var norm = href.replace(/\/$/, "") || "/index.html";
    if (norm === path) a.classList.add("active");
  });

  // Homepage uptime counter
  var up = document.querySelector("[data-uptime]");
  if (up) {
    var started = Date.now();
    function tick() {
      var s = Math.floor((Date.now() - started) / 1000);
      var h = String(Math.floor(s / 3600)).padStart(2, "0");
      var m = String(Math.floor((s % 3600) / 60)).padStart(2, "0");
      var ss = String(s % 60).padStart(2, "0");
      up.textContent = h + ":" + m + ":" + ss;
    }
    tick();
    setInterval(tick, 1000);
  }

  // Forms demo: live JSON preview
  var pf = document.querySelector("[data-preview-form]");
  if (pf) {
    var preview = document.querySelector("[data-preview-output]");
    function render() {
      var data = {};
      new FormData(pf).forEach(function (v, k) { data[k] = v; });
      if (preview) preview.textContent = JSON.stringify(data, null, 2);
    }
    pf.addEventListener("input", render);
    render();
  }

  // DELETE demo
  var delBtn = document.querySelector("[data-delete-btn]");
  if (delBtn) {
    var delInput = document.querySelector("[data-delete-path]");
    var delOut = document.querySelector("[data-delete-result]");
    delBtn.addEventListener("click", async function () {
      var p = (delInput.value || "").trim();
      if (!p) {
        delOut.innerHTML = '<div class="status-line"><span class="badge warn">empty</span></div>Please enter a resource path.';
        delOut.classList.remove("empty");
        return;
      }
      delOut.textContent = "Sending DELETE " + p + " ...";
      delOut.classList.remove("empty");
      try {
        var res = await fetch(p, { method: "DELETE" });
        var text = "";
        try { text = await res.text(); } catch (e) {}
        var cls = res.ok ? "ok" : (res.status >= 500 ? "err" : "warn");
        delOut.innerHTML =
          '<div class="status-line">' +
            '<span class="badge ' + cls + '">' + res.status + ' ' + res.statusText + '</span>' +
            '<span style="color:var(--muted)">DELETE ' + escapeHtml(p) + '</span>' +
          '</div>' +
          (text ? escapeHtml(text) : '<span style="color:var(--muted)">(empty response body)</span>');
      } catch (err) {
        delOut.innerHTML =
          '<div class="status-line"><span class="badge err">network error</span></div>' +
          escapeHtml(String(err));
      }
    });
  }

  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
    });
  }
})();
