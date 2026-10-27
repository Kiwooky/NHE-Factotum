function (event, funcs) {
    // Factotum face script. ES5 and small on purpose: if a face script throws,
    // mod-ui silently disables it.
    //  - Light panel: opacity follows the plugin's Glow output (0..1, already eased and
    //    dark while bypassed), never below FLOOR so the board stays faintly visible.
    //  - Starve swaps the panel art: red/orange off, yellow on.
    //  - Octave LED follows the Octave switch (the bypass LED is mod-ui's bypass-light).
    var icon = event.icon;
    var d = event.data;
    var FLOOR = 0.12;

    function clamp01(x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
    function num(v, def) { v = parseFloat(v); return isNaN(v) ? def : v; }

    function draw(sym) {
        var all = !sym;
        if (all || sym == 'glow') {
            var g = clamp01(num(d.glow, 0));
            icon.find('.ft-panel').css('opacity', FLOOR + (1 - FLOOR) * g);
        }
        if (all || sym == 'starve') {
            icon.find('.ft-panel').toggleClass('starve', num(d.starve, 0) > 0.5);
        }
        if (all || sym == 'octave') {
            icon.find('.ft-led-octave').toggleClass('on', num(d.octave, 0) > 0.5);
        }
    }

    if (event.type == 'start') {
        var ports = event.ports || [];
        for (var i = 0; i < ports.length; i++) d[ports[i].symbol] = ports[i].value;
        draw();
    } else if (event.type == 'change') {
        d[event.symbol] = event.value;
        draw(event.symbol);
    }
}
