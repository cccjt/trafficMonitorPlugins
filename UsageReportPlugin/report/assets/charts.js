(function () {
  var D = window.REPORT_DATA;
  if (!D) return;

  var css = getComputedStyle(document.documentElement);
  var accent = css.getPropertyValue('--accent').trim();
  var accent2 = css.getPropertyValue('--accent2').trim();
  var ink = css.getPropertyValue('--ink').trim();
  var muted = css.getPropertyValue('--muted').trim();
  var rule = css.getPropertyValue('--rule').trim();
  var bg2 = css.getPropertyValue('--bg2').trim();

  var WD = ['周日', '周一', '周二', '周三', '周四', '周五', '周六'];
  var charts = [];

  /* ---------- fill dynamic numbers from data ---------- */
  (function () {
    var s = D.summary || {};
    function fmtH(min) { return ((min || 0) / 60).toFixed(1); }
    function setAll(key, val) {
      var nodes = document.querySelectorAll('[data-kpi="' + key + '"]');
      for (var i = 0; i < nodes.length; i++) { nodes[i].textContent = val; }
    }
    if (s.from) setAll('from', s.from);
    if (s.to) setAll('to', s.to);
    if (s.days) setAll('days', s.days);
    if (s.generatedAt) setAll('generatedAt', s.generatedAt);
    setAll('avg', fmtH(s.avgMinutes));
    setAll('median', fmtH(s.medianMinutes));
    setAll('longest', fmtH(s.longestMinutes));
    if (s.totalHours !== undefined) setAll('total', Math.round(s.totalHours));
    if (s.onlineHours !== undefined) setAll('online', Math.round(s.onlineHours));
    if (s.lockedHours !== undefined) setAll('locked', Math.round(s.lockedHours));
    if (s.locks !== undefined) setAll('locks', s.locks);
    if (s.zeroDays !== undefined) setAll('zero', s.zeroDays);
    if (s.lockedHours !== undefined && s.onlineHours) {
      setAll('lockPct', (s.lockedHours / s.onlineHours * 100).toFixed(1));
      setAll('deduct', Math.round(s.onlineHours - (s.totalHours || 0)));
    }
  })();

  function make(id) {
    var el = document.getElementById(id);
    if (!el) return null;
    var c = echarts.init(el, null, { renderer: 'svg' });
    charts.push(c);
    return c;
  }

  function axisStyle() {
    return {
      axisLine: { lineStyle: { color: rule } },
      axisTick: { show: false },
      splitLine: { lineStyle: { color: rule } },
      axisLabel: { color: muted, fontSize: 11 }
    };
  }

  function tooltipBase() {
    return {
      appendToBody: true,
      backgroundColor: bg2,
      borderColor: rule,
      textStyle: { color: ink, fontSize: 12 }
    };
  }

  /* ---------- Chart 1: daily usage (stacked) ---------- */
  (function () {
    var c = make('chart-daily');
    if (!c) return;
    var dates = D.days.map(function (d) { return d.d; });
    var useVals = D.days.map(function (d) { return +(d.minutes / 60).toFixed(2); });
    var lockVals = D.days.map(function (d) { return +((d.locked || 0) / 60).toFixed(2); });
    var offVals = D.days.map(function (d) { return +((d.off || 0) / 60).toFixed(2); });
    var avg = +((D.summary.avgMinutes / 60).toFixed(1));
    var dayCount = D.days.length;
    var zoomStart = dayCount > 7 ? +(((dayCount - 7) / dayCount) * 100).toFixed(2) : 0;

    c.setOption({
      animation: false,
      grid: { left: 52, right: 92, top: 26, bottom: 68 },
      tooltip: {
        trigger: 'axis',
        axisPointer: { type: 'shadow' },
        appendToBody: true,
        backgroundColor: bg2,
        borderColor: rule,
        textStyle: { color: ink, fontSize: 12 },
        formatter: function (p) {
          var day = D.days[p[0].dataIndex];
          return '<b>' + day.d + '</b> ' + WD[day.dow] +
            '<br/>使用 ' + (day.minutes / 60).toFixed(1) + ' 小时' +
            '<br/>锁屏 ' + ((day.locked || 0) / 60).toFixed(1) + ' 小时' +
            '<br/>息屏 ' + ((day.off || 0) / 60).toFixed(1) + ' 小时' +
            '<br/>开机合计 ' + (day.online / 60).toFixed(1) + ' 小时' +
            '<br/>最长一段 ' + (day.maxSess / 60).toFixed(1) + ' 小时';
        }
      },
      xAxis: (function () {
        var a = axisStyle();
        a.type = 'category';
        a.data = dates;
        a.axisLabel = {
          color: muted, fontSize: 11, rotate: 45, interval: 'auto',
          formatter: function (v) { return v.slice(5); }
        };
        return a;
      })(),
      yAxis: (function () {
        var a = axisStyle();
        a.type = 'value';
        a.max = 24;
        return a;
      })(),
      dataZoom: [
        {
          type: 'slider', start: zoomStart, end: 100, height: 18, bottom: 10,
          borderColor: rule, fillerColor: accent + '22',
          handleStyle: { color: accent },
          textStyle: { color: muted, fontSize: 10 },
          dataBackground: { lineStyle: { color: rule }, areaStyle: { color: rule } }
        }
      ],
      series: [
        {
          name: '使用中',
          type: 'bar',
          stack: 'total',
          data: useVals,
          barWidth: '70%',
          itemStyle: { color: accent },
          markLine: {
            silent: true, symbol: 'none',
            lineStyle: { color: muted, type: 'dashed', width: 1 },
            label: { color: muted, fontSize: 10, formatter: '日均 ' + avg + 'h', position: 'end', distance: 6 },
            data: [{ yAxis: avg }]
          }
        },
        {
          name: '开机但锁屏',
          type: 'bar',
          stack: 'total',
          data: lockVals,
          barWidth: '70%',
          itemStyle: { color: accent2, opacity: 0.55 }
        },
        {
          name: '开机但息屏',
          type: 'bar',
          stack: 'total',
          data: offVals,
          barWidth: '70%',
          itemStyle: { color: muted, opacity: 0.4 }
        }
      ]
    });
  })();

  /* ---------- Chart: daily first-start / last-end time ---------- */
  (function () {
    var c = make('chart-times');
    if (!c) return;
    var dates = D.days.map(function (d) { return d.d; });
    var startVals = D.days.map(function (d) {
      return d.startMin === null || d.startMin === undefined ? null : +(d.startMin / 60).toFixed(2);
    });
    var endVals = D.days.map(function (d) {
      return d.endMin === null || d.endMin === undefined ? null : +(d.endMin / 60).toFixed(2);
    });
    var zoomStart2 = D.days.length > 7 ? +(((D.days.length - 7) / D.days.length) * 100).toFixed(2) : 0;

    function fmtClock(v) {
      if (v === null || v === undefined) return '—';
      var total = Math.round(v * 60);
      var h = Math.floor(total / 60) % 24;
      var m = total % 60;
      return (h < 10 ? '0' : '') + h + ':' + (m < 10 ? '0' : '') + m;
    }

    c.setOption({
      animation: false,
      grid: { left: 56, right: 26, top: 20, bottom: 56 },
      tooltip: {
        trigger: 'axis',
        appendToBody: true,
        backgroundColor: bg2,
        borderColor: rule,
        textStyle: { color: ink, fontSize: 12 },
        formatter: function (p) {
          var day = D.days[p[0].dataIndex];
          var s = '—', e = '—';
          p.forEach(function (item) {
            if (item.seriesName === '开机') s = fmtClock(item.value);
            if (item.seriesName === '关机') e = fmtClock(item.value);
          });
          return '<b>' + day.d + '</b><br/>开机 ' + s + '<br/>关机 ' + e + (day.endNext ? '（次日）' : '');
        }
      },
      xAxis: (function () {
        var a = axisStyle();
        a.type = 'category';
        a.data = dates;
        a.axisLabel = {
          color: muted, fontSize: 11, rotate: 45, interval: 'auto',
          formatter: function (v) { return v.slice(5); }
        };
        return a;
      })(),
      yAxis: (function () {
        var a = axisStyle();
        a.type = 'value';
        a.min = 0;
        a.max = 24;
        a.interval = 3;
        a.axisLabel = {
          color: muted, fontSize: 11,
          formatter: function (v) { return (v < 10 ? '0' + v : v) + ':00'; }
        };
        return a;
      })(),
      dataZoom: [
        {
          type: 'slider', start: zoomStart2, end: 100, height: 16, bottom: 8,
          borderColor: rule, fillerColor: accent + '22',
          handleStyle: { color: accent },
          textStyle: { color: muted, fontSize: 10 },
          dataBackground: { lineStyle: { color: rule }, areaStyle: { color: rule } }
        }
      ],
      series: [
        {
          name: '开机',
          type: 'line',
          data: startVals,
          connectNulls: false,
          symbol: 'circle',
          symbolSize: 4,
          lineStyle: { width: 1.5, color: accent },
          itemStyle: { color: accent }
        },
        {
          name: '关机',
          type: 'line',
          data: endVals,
          connectNulls: false,
          symbol: 'circle',
          symbolSize: 4,
          lineStyle: { width: 1.5, color: accent2 },
          itemStyle: { color: accent2 }
        }
      ]
    });
  })();

  /* ---------- Calendar: usage + sleep, one month at a time ---------- */
  (function () {
    var host = document.getElementById('calendar');
    var title = document.getElementById('cal-title');
    var prev = document.getElementById('cal-prev');
    var next = document.getElementById('cal-next');
    if (!host || !title || !prev || !next) return;

    var DOW = ['周一', '周二', '周三', '周四', '周五', '周六', '周日'];
    var map = {};
    D.days.forEach(function (d) { map[d.d] = d; });

    function hexToRgb(hex) {
      var h = hex.replace('#', '');
      if (h.length === 3) { h = h[0] + h[0] + h[1] + h[1] + h[2] + h[2]; }
      var n = parseInt(h, 16);
      return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
    }
    var rgb = hexToRgb(accent);

    var first = D.days[0].d.split('-');
    var last = D.days[D.days.length - 1].d.split('-');
    var minKey = (+first[0]) * 12 + (+first[1] - 1);
    var maxKey = (+last[0]) * 12 + (+last[1] - 1);
    var curKey = maxKey; /* 默认显示最新月份 */

    function pad(n) { return n < 10 ? '0' + n : '' + n; }

    function render() {
      var y = Math.floor(curKey / 12);
      var m = curKey % 12;
      var monthDays = new Date(y, m + 1, 0).getDate();
      var startDow = (new Date(y, m, 1).getDay() + 6) % 7; /* Monday = 0 */

      title.textContent = y + ' 年 ' + (m + 1) + ' 月';
      prev.disabled = curKey <= minKey;
      next.disabled = curKey >= maxKey;

      var html = '<div class="cal-scroll"><div class="cal-grid">';
      DOW.forEach(function (d) { html += '<div class="cal-dow">' + d + '</div>'; });
      for (var s = 0; s < startDow; s++) { html += '<div></div>'; }

      for (var day = 1; day <= monthDays; day++) {
        var key = y + '-' + pad(m + 1) + '-' + pad(day);
        var rec = map[key];
        if (!rec) {
          html += '<div class="cal-cell blank"><span class="cal-day">' + day + '</span></div>';
          continue;
        }
        var ratio = rec.minutes / 1440;
        var bg = rec.minutes
          ? ' style="background: rgba(' + rgb[0] + ',' + rgb[1] + ',' + rgb[2] + ',' + (0.04 + 0.26 * ratio).toFixed(3) + ')"'
          : '';
        var cls = 'cal-cell has-data' + (rec.minutes ? '' : ' zero');
        html += '<div class="' + cls + '" data-date="' + key + '"' + bg + '>' +
          '<span class="cal-day">' + day + '</span>' +
          '<span class="cal-use">' + (rec.minutes ? (rec.minutes / 60).toFixed(1) + '<i>h</i>' : '—') + '</span>' +
          '<span class="cal-sleep">睡 ' + (rec.sleep ? (rec.sleep / 60).toFixed(1) + '<i>h</i>' : '—') + '</span>' +
          '</div>';
      }
      html += '</div></div>';
      host.innerHTML = html;
    }

    prev.addEventListener('click', function () {
      if (curKey > minKey) { curKey--; render(); }
    });
    next.addEventListener('click', function () {
      if (curKey < maxKey) { curKey++; render(); }
    });

    /* ---- detail modal ---- */
    var modal = document.getElementById('cal-modal');
    var modalDate = document.getElementById('modal-date');
    var modalTl = document.getElementById('modal-timeline');
    var modalClose = document.getElementById('modal-close');

    function openModal(iso) {
      var rec = map[iso];
      if (!rec || !modal) return;
      var dt = new Date(iso + 'T00:00:00');
      modalDate.textContent = iso + '　' + DOW[(dt.getDay() + 6) % 7];

      var rows = '';
      if (!rec.startAt) {
        rows = '<div class="tl-empty">当天没有使用记录。</div>';
      } else {
        rows += '<div class="tl-row">' +
          '<span class="tl-time">' + rec.startAt + '</span>' +
          '<span class="tl-label"><b style="background:' + accent + '"></b>开机</span>' +
          '</div>';
        rows += '<div class="tl-row">' +
          '<span class="tl-time">' + (rec.endAt || '—') + '</span>' +
          '<span class="tl-label"><b style="background:' + accent2 + '"></b>关机' +
          (rec.endNext ? '（次日）' : '') + '</span>' +
          '</div>';
      }
      modalTl.innerHTML = rows;
      modal.classList.add('open');
      modal.setAttribute('aria-hidden', 'false');
    }

    function closeModal() {
      if (!modal) return;
      modal.classList.remove('open');
      modal.setAttribute('aria-hidden', 'true');
    }

    host.addEventListener('click', function (ev) {
      var cell = ev.target.closest ? ev.target.closest('.cal-cell.has-data') : null;
      if (cell) openModal(cell.getAttribute('data-date'));
    });

    if (modalClose) modalClose.addEventListener('click', closeModal);
    if (modal) {
      modal.addEventListener('click', function (ev) {
        if (ev.target && ev.target.getAttribute('data-close')) closeModal();
      });
    }
    document.addEventListener('keydown', function (ev) {
      if (ev.key === 'Escape') closeModal();
    });

    render();
  })();

  window.addEventListener('resize', function () {
    charts.forEach(function (c) { c.resize(); });
  });
})();
