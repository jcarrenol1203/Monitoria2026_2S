/* Calculadoras del artifact de monitores: PSC/ARR/CCR1 y filtro digital ICxF */
(function () {
	var $ = function (i) { return document.getElementById(i); };

	/* Tabla de la pagina del TIMx_CCMR1 (RM0383): [divisor de f_DTS (0 = f_CK_INT), N] */
	var FILTER = [
		[0, 0], [0, 2], [0, 4], [0, 8],
		[2, 6], [2, 8], [4, 6], [4, 8],
		[8, 6], [8, 8], [16, 5], [16, 6],
		[16, 8], [32, 5], [32, 6], [32, 8]
	];

	function fmtTime(s) {
		if (s === 0) { return '0'; }
		if (s < 1e-6) { return (s * 1e9).toFixed(0) + ' ns'; }
		if (s < 1e-3) { return (s * 1e6).toFixed(s < 1e-5 ? 2 : 1).replace(/\.?0+$/, '') + ' µs'; }
		return (s * 1e3).toFixed(2) + ' ms';
	}

	function fmtFreq(f) {
		if (f >= 1e6) { return (f / 1e6).toFixed(2).replace(/\.?0+$/, '') + ' MHz'; }
		if (f >= 1e3) { return (f / 1e3).toFixed(1).replace(/\.0$/, '') + ' kHz'; }
		return f.toFixed(1).replace(/\.0$/, '') + ' Hz';
	}

	function calcPwm() {
		var clk = +$('clk').value * 1e6, psc = +$('psc').value, f = +$('freq').value, d = +$('duty').value;
		var tick = (psc + 1) / clk, arr = Math.round(1 / (f * tick)) - 1, note = '';
		if (!(arr >= 1) || !isFinite(arr)) {
			$('o-arr').textContent = '—'; $('o-ccr').textContent = '—'; $('o-real').textContent = '—';
			$('o-tick').textContent = fmtTime(tick);
			$('o-note').textContent = 'Frecuencia fuera de rango para ese PSC.';
			return;
		}
		if (arr > 65535) { note = 'TIM1 es de 16 bit: ARR no puede pasar de 65535, sube el PSC.'; }
		var ccr = Math.round((arr + 1) * d / 100);
		$('o-tick').textContent = fmtTime(tick);
		$('o-arr').textContent = arr;
		$('o-ccr').textContent = ccr;
		$('o-real').textContent = fmtFreq(1 / ((arr + 1) * tick));
		$('o-note').textContent = note || 'ARR = clk / ((PSC+1) · f) − 1 · CCR1 = (ARR+1) · duty / 100';
	}

	function calcFilter() {
		var fclk = +$('fclk').value * 1e6, ckd = +$('ckd').value, code = +$('icf').value;
		var fdts = fclk / ckd, div = FILTER[code][0], n = FILTER[code][1];
		if (n === 0) {
			$('f-fs').textContent = '—'; $('f-n').textContent = '—'; $('f-t').textContent = '0';
			$('f-note').textContent = 'Sin filtro: cualquier cambio de nivel llega al timer.';
			return;
		}
		var fs = div === 0 ? fclk : fdts / div;
		var t = n / fs;
		$('f-fs').textContent = fmtFreq(fs);
		$('f-n').textContent = n;
		$('f-t').textContent = fmtTime(t);
		$('f-note').textContent = 'Un rebote mecánico dura 1 a 10 ms: ' + (t < 1e-3 ? 'este filtro es entre ' + Math.round(1e-3 / t) + ' y ' + Math.round(1e-2 / t) + ' veces más corto, por eso no lo elimina.' : 'este filtro ya es comparable con el rebote.');
	}

	var sel = $('icf');
	FILTER.forEach(function (_, i) {
		var o = document.createElement('option');
		o.value = i;
		o.textContent = ('0000' + i.toString(2)).slice(-4);
		sel.appendChild(o);
	});
	sel.value = 15;

	['clk', 'psc', 'freq', 'duty'].forEach(function (i) { $(i).addEventListener('input', calcPwm); });
	['fclk', 'ckd', 'icf'].forEach(function (i) { $(i).addEventListener('input', calcFilter); });
	calcPwm();
	calcFilter();
})();
