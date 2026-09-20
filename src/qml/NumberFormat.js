.pragma library

// 数值显示工具：把 double 转成普通小数文本，避免出现科学计数法（如 1e-08 / 1.23e-09）。
// 用于容差、偏差等小量级数值的显示与输入框初值。

// 把数值转成不使用科学计数法的字符串；非数值原样返回。
function toPlain(value) {
    var n = Number(value)
    if (!isFinite(n)) return String(value)
    var text = String(n)
    var match = /^(-?)(\d+)(?:\.(\d+))?[eE]([+-]?\d+)$/.exec(text)
    if (!match) return text
    var sign = match[1]
    var intPart = match[2]
    var fracPart = match[3] || ""
    var digits = intPart + fracPart
    var pointIndex = intPart.length + parseInt(match[4], 10)
    if (pointIndex <= 0) {
        var leading = ""
        for (var i = 0; i < -pointIndex; ++i) leading += "0"
        return sign + "0." + leading + digits
    }
    if (pointIndex >= digits.length) {
        var trailing = ""
        for (var j = 0; j < pointIndex - digits.length; ++j) trailing += "0"
        return sign + digits + trailing
    }
    return sign + digits.substring(0, pointIndex) + "." + digits.substring(pointIndex)
}

// 在十进制字符串上移动小数点（places 为正表示放大）。
// 直接对 double 乘除 10 会带出二进制浮点误差：1e-8 * 10 是 1.0000000000000001e-7，
// 展开成普通小数就变成 0.00000010000000000000001，看起来“不是十倍”。
// 所以在十进制文本上移位，保证点一下箭头就是干净的 10 倍 / 十分之一。
function shiftDecimal(text, places) {
    var negative = text.charAt(0) === "-"
    var body = negative ? text.substring(1) : text
    var dot = body.indexOf(".")
    if (dot < 0) {
        body += "."
        dot = body.length - 1
    }
    var digits = body.substring(0, dot) + body.substring(dot + 1)
    var pointIndex = dot + places
    var result
    if (pointIndex <= 0) {
        var leading = ""
        for (var i = 0; i < -pointIndex; ++i) leading += "0"
        result = "0." + leading + digits
    } else if (pointIndex >= digits.length) {
        var trailing = ""
        for (var j = 0; j < pointIndex - digits.length; ++j) trailing += "0"
        result = digits + trailing
    } else {
        result = digits.substring(0, pointIndex) + "." + digits.substring(pointIndex)
    }
    result = result.replace(/^0+(?=\d)/, "")   // 去掉整数部分多余的前导 0
    return (negative ? "-" : "") + result
}

// 固定宽度里放不下时用的简略写法：仍用普通小数，超出长度才退回紧凑指数写法，
// 完整数值请配合 ToolTip 用 toPlain() 展示。
function toCompact(value, maxChars) {
    var plain = toPlain(value)
    var limit = (maxChars === undefined || maxChars === null) ? 14 : maxChars
    if (plain.length <= limit) return plain
    var n = Number(value)
    if (!isFinite(n) || n === 0) return plain
    return n.toExponential(3)
}
