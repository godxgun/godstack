(() => {
  "use strict";

  // Nudge the light's origin; CSS supplies the slow atmospheric motion.
  if (matchMedia("(hover: hover) and (prefers-reduced-motion: no-preference)").matches) {
    document.querySelectorAll(".heaven-button").forEach((light) => {
      light.addEventListener("pointermove", (event) => {
        const bounds = light.getBoundingClientRect();
        const x = (event.clientX - bounds.left - bounds.width / 2) * .18;
        const y = (event.clientY - bounds.top - bounds.height / 2) * .3;
        light.style.setProperty("--light-x", `${x.toFixed(1)}px`);
        light.style.setProperty("--light-y", `${y.toFixed(1)}px`);
      });
      light.addEventListener("pointerleave", () => {
        light.style.setProperty("--light-x", "0px");
        light.style.setProperty("--light-y", "0px");
      });
    });
  }

  const menu = document.querySelector(".menu-toggle");
  const sidebar = document.querySelector(".sidebar");
  let priorFocus = null;

  function closeMenu() {
    if (!sidebar?.classList.contains("is-open")) return;
    sidebar.classList.remove("is-open");
    menu.setAttribute("aria-expanded", "false");
    priorFocus?.focus();
    priorFocus = null;
  }

  menu?.addEventListener("click", () => {
    if (menu.getAttribute("aria-expanded") === "true") {
      closeMenu();
      return;
    }
    priorFocus = menu;
    sidebar.classList.add("is-open");
    menu.setAttribute("aria-expanded", "true");
    sidebar.querySelector("a")?.focus();
  });

  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") closeMenu();
    if (event.key !== "Tab" || !sidebar?.classList.contains("is-open")) return;

    const focusable = [...sidebar.querySelectorAll('a[href], summary, button:not([disabled])')].filter(element => element.getClientRects().length);
    if (!focusable.length) {
      event.preventDefault();
      return;
    }
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    if (event.shiftKey && document.activeElement === first) {
      event.preventDefault();
      last.focus();
    } else if (!event.shiftKey && document.activeElement === last) {
      event.preventDefault();
      first.focus();
    }
  });

  document.addEventListener("click", (event) => {
    if (sidebar?.classList.contains("is-open") &&
        !sidebar.contains(event.target) && !menu.contains(event.target)) {
      closeMenu();
    }
  });

  const dialog = document.querySelector(".search-dialog");
  const openSearch = document.querySelector(".search-open");
  const input = document.querySelector("#search-input");
  const results = document.querySelector(".search-results");
  const status = document.querySelector(".search-status");
  let searchOpener = null;
  let searchIndex = null;
  let pendingIndex = null;
  let searchRevision = 0;

  function showSearch() {
    searchOpener = document.activeElement;
    if (!dialog.open) dialog.showModal();
    input.focus();
  }

  openSearch?.addEventListener("click", showSearch);
  dialog?.addEventListener("close", () => searchOpener?.focus());
  document.addEventListener("keydown", (event) => {
    if (event.key === "Tab" && dialog?.open) {
      const focusable = [...dialog.querySelectorAll('a[href], button:not([disabled]), input:not([disabled]), [tabindex]:not([tabindex="-1"])')]
        .filter(element => element.getClientRects().length);
      const first = focusable[0];
      const last = focusable[focusable.length - 1];
      if (first && (!dialog.contains(document.activeElement) ||
          event.shiftKey && document.activeElement === first ||
          !event.shiftKey && document.activeElement === last)) {
        event.preventDefault();
        (event.shiftKey ? last : first).focus();
      }
    }
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") {
      event.preventDefault();
      showSearch();
    }
  });

  const searchURL = '{{ .searchURL }}';
  const docsArticle = document.querySelector("[data-symbol-vault-url]");
  if (docsArticle && window.location.hash) {
    let fragment = window.location.hash.slice(1);
    try {
      fragment = decodeURIComponent(fragment);
    } catch {
      // Keep the original fragment if it is not valid URL encoding.
    }
    if (fragment && !document.getElementById(fragment)) {
      const vaultURL = docsArticle.dataset.symbolVaultUrl;
      window.location.replace(`${vaultURL}${window.location.search}#${encodeURIComponent(fragment)}`);
    }
  }

  input?.addEventListener("input", async () => {
    const revision = ++searchRevision;
    const query = input.value.trim().toLocaleLowerCase();
    results.replaceChildren();
    if (!query) {
      status.textContent = "";
      return;
    }

    if (!searchIndex) {
      status.textContent = "Loading search index…";
      if (!pendingIndex) {
        pendingIndex = fetch(searchURL)
          .then((response) => {
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.json();
          })
          .then((data) => {
            if (!Array.isArray(data)) throw new Error("Invalid search index");
            return data;
          })
          .catch((error) => {
            pendingIndex = null;
            throw error;
          });
      }
      try {
        searchIndex = await pendingIndex;
      } catch {
        if (revision === searchRevision) {
          status.textContent = "Search index could not be loaded. Check your connection and try again.";
        }
        return;
      }
    }

    // An input event may have superseded this handler during the fetch.
    if (revision !== searchRevision) return;
    const latestQuery = input.value.trim().toLocaleLowerCase();
    if (!latestQuery) {
      status.textContent = "";
      return;
    }

    const rank = (title) => {
      const normalized = title.trim().toLocaleLowerCase();
      if (normalized === latestQuery) return 0;
      if (normalized.startsWith(latestQuery)) return 1;
      return 2;
    };
    const matches = searchIndex.filter((item) => {
      const searchable = `${item.title || ""} ${item.description || ""} ${item.content || ""} ${(item.tags || []).join(" ")} ${item.category || ""} ${item.product || ""}`;
      return searchable.toLocaleLowerCase().includes(latestQuery);
    }).sort((left, right) => rank(left.title || "") - rank(right.title || "")).slice(0, 12);

    status.textContent = matches.length
      ? `Showing ${matches.length} matching page${matches.length === 1 ? "" : "s"}.`
      : "No matching pages.";
    for (const item of matches) {
      const link = document.createElement("a");
      link.href = item.url;
      link.textContent = item.title || "Untitled";
      const description = document.createElement("p");
      description.textContent = item.description || item.category || "";
      link.append(description);
      results.append(link);
    }
  });

  document.querySelectorAll("[data-web-demo]").forEach((demo) => {
    const start = demo.querySelector("[data-demo-start]");
    const stop = demo.querySelector("[data-demo-stop]");
    const frame = demo.querySelector("[data-demo-frame]");
    const status = demo.querySelector("[data-demo-status]");

    start.addEventListener("click", () => {
      if (!frame.hidden) return;
      const iframe = document.createElement("iframe");
      iframe.title = `${frame.dataset.demoTitle} WebAssembly demo`;
      iframe.loading = "eager";
      iframe.referrerPolicy = "no-referrer";
      iframe.setAttribute("sandbox", "allow-scripts");
      iframe.src = frame.dataset.demoSrc;
      frame.replaceChildren(iframe);
      frame.hidden = false;
      start.disabled = true;
      stop.disabled = false;
      status.textContent = "Demo frame opened. Output appears inside the embedded frame.";
    });

    stop.addEventListener("click", () => {
      const iframe = frame.querySelector("iframe");
      if (iframe) iframe.src = "about:blank";
      frame.replaceChildren();
      frame.hidden = true;
      start.disabled = false;
      stop.disabled = true;
      status.textContent = "Demo stopped and unloaded.";
      start.focus();
    });
  });

  document.querySelectorAll("pre").forEach((pre) => {
    const button = document.createElement("button");
    button.className = "copy-code";
    button.type = "button";
    button.textContent = "Copy";
    button.setAttribute("aria-label", "Copy code to clipboard");
    button.addEventListener("click", async () => {
      try {
        await navigator.clipboard.writeText(pre.querySelector("code")?.innerText || pre.innerText);
        button.textContent = "Copied";
        button.setAttribute("aria-label", "Code copied");
      } catch {
        button.textContent = "Unable to copy";
        button.setAttribute("aria-label", "Copy failed");
      }
      setTimeout(() => {
        button.textContent = "Copy";
        button.setAttribute("aria-label", "Copy code to clipboard");
      }, 1800);
    });
    const symbolCard = pre.closest(".symbol-card");
    if (symbolCard) {
      button.classList.add("symbol-copy");
      symbolCard.append(button);
    } else {
      let block = pre.parentElement.matches(".highlight") ? pre.parentElement : null;
      if (!block) {
        block = document.createElement("div");
        pre.before(block);
        block.append(pre);
      }
      block.classList.add("code-block");
      block.append(button);
    }
  });
})();
