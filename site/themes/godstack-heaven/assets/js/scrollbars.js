(() => {
  "use strict";
  const root = document.documentElement;
  const layer = document.createElement("div");
  layer.className = "scrollbar-overlay";
  layer.setAttribute("aria-hidden", "true");
  document.body.append(layer);
  const entries = new Map();
  let pointer = null, frame = 0, drag = null;
  const edge = 96;
  const schedule = () => { if (!frame) frame = requestAnimationFrame(update); };

  function add(element) {
    if (entries.has(element)) return;
    const thumbs = {};
    for (const axis of ["x", "y"]) {
      const thumb = document.createElement("div");
      thumb.className = "scrollbar-thumb";
      thumb.scroller = element;
      layer.append(thumb);
      thumbs[axis] = thumb;
      thumb.addEventListener("pointerdown", (event) => {
        if (event.button !== 0) return;
        event.preventDefault();
        const dimension = axis === "y" ? "Height" : "Width";
        const rect = element === root ? { width: innerWidth, height: innerHeight } : element.getBoundingClientRect();
        const viewport = rect[dimension.toLowerCase()];
        const length = parseFloat(thumb.style[dimension.toLowerCase()]);
        drag = { element, axis, thumb, origin: axis === "y" ? event.clientY : event.clientX,
          start: element[axis === "y" ? "scrollTop" : "scrollLeft"],
          scale: (element[`scroll${dimension}`] - viewport) / Math.max(1, viewport - 8 - length),
          behavior: element.style.scrollBehavior };
        element.style.scrollBehavior = "auto";
        thumb.setPointerCapture(event.pointerId);
        schedule();
      });
      const release = () => {
        if (drag?.thumb !== thumb) return;
        drag.element.style.scrollBehavior = drag.behavior;
        drag = null;
        schedule();
      };
      thumb.addEventListener("pointerup", release);
      thumb.addEventListener("pointercancel", release);
      thumb.addEventListener("lostpointercapture", release);
    }
    entries.set(element, thumbs);
  }

  function update() {
    frame = 0;
    // A modal owns the browser's top layer; its thumbs must live there too.
    const modal = document.querySelector("dialog[open]");
    const host = modal || document.body;
    if (layer.parentElement !== host) host.append(layer);
    const origin = layer.getBoundingClientRect();
    for (const [element, thumbs] of entries) {
      if (!element.isConnected) {
        Object.values(thumbs).forEach(thumb => thumb.remove());
        entries.delete(element);
        continue;
      }
      const isRoot = element === root;
      const rect = isRoot ? { left:0, top:0, right:innerWidth, bottom:innerHeight } : element.getBoundingClientRect();
      const styles = isRoot ? null : getComputedStyle(element);
      for (const axis of ["x", "y"]) {
        const vertical = axis === "y";
        const viewport = isRoot ? (vertical ? innerHeight : innerWidth) : element[vertical ? "clientHeight" : "clientWidth"];
        const total = element[vertical ? "scrollHeight" : "scrollWidth"];
        const overflow = isRoot || /auto|scroll|overlay/.test(styles[vertical ? "overflowY" : "overflowX"]);
        const visible = (!modal || modal.contains(element)) && viewport > 8 && total > viewport + 1 && overflow && rect.bottom > 0 && rect.top < innerHeight && rect.right > 0 && rect.left < innerWidth;
        const distance = pointer ? (vertical ? rect.right - pointer.x : rect.bottom - pointer.y) : Infinity;
        const within = pointer && pointer.x >= rect.left && pointer.x <= rect.right && pointer.y >= rect.top && pointer.y <= rect.bottom;
        const active = drag?.element === element && drag.axis === axis;
        const alpha = visible && (active || within && distance >= 0 && distance < edge) ? (active ? .8 : .6 * (1 - distance / edge)) : 0;
        const thumb = thumbs[axis];
        thumb.style.opacity = alpha;
        thumb.classList.toggle("is-visible", alpha > 0);
        if (!visible) continue;
        const track = viewport - 8;
        const length = Math.min(track, Math.max(28, track * viewport / total));
        const scroll = element[vertical ? "scrollTop" : "scrollLeft"];
        const offset = Math.max(0, Math.min(1, scroll / (total - viewport))) * (track - length);
        Object.assign(thumb.style, vertical
          ? { left:`${rect.right - origin.left - 8}px`, top:`${rect.top - origin.top + 4 + offset}px`, width:"4px", height:`${length}px` }
          : { left:`${rect.left - origin.left + 4 + offset}px`, top:`${rect.bottom - origin.top - 8}px`, width:`${length}px`, height:"4px" });
      }
    }
  }

  add(root);
  document.addEventListener("pointermove", (event) => {
    pointer = event.pointerType === "mouse" ? { x:event.clientX, y:event.clientY } : null;
    if (drag) {
      drag.element[drag.axis === "y" ? "scrollTop" : "scrollLeft"] = drag.start + ((drag.axis === "y" ? event.clientY : event.clientX) - drag.origin) * drag.scale;
    } else if (pointer) {
      let element = event.target.scroller || event.target;
      while (element instanceof Element && element !== root) {
        if (element.scrollHeight > element.clientHeight + 1 || element.scrollWidth > element.clientWidth + 1) add(element);
        element = element.parentElement;
      }
    }
    schedule();
  }, { passive:true });
  document.addEventListener("pointerout", event => { if (!event.relatedTarget) { pointer = null; schedule(); } });
  addEventListener("blur", () => { pointer = null; schedule(); });
  document.addEventListener("scroll", schedule, { capture:true, passive:true });
  addEventListener("resize", schedule);
  new ResizeObserver(schedule).observe(document.body);
  new MutationObserver(schedule).observe(document.querySelector("main"), { childList:true, subtree:true, attributes:true, attributeFilter:["open", "hidden"] });
  document.querySelectorAll("dialog").forEach(dialog => {
    new MutationObserver(schedule).observe(dialog, { attributes:true, attributeFilter:["open"] });
  });
  schedule();
})();
