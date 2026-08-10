
class Char {
	constructor(x, y, idx = 32) {
		this.x = x
		this.y = y
		this.idx = idx
		this.fgColor = [1, 1, 1]
		this.bgColor = [0, 0, 0]
		Object.preventExtensions(this)
	}
	cloneFrom(other) {
		this.fgColor = other.fgColor
		this.bgColor = other.bgColor
		this.idx = other.idx
		return this
	}
}
class Grid2 {
	constructor(w, h, type = Array) {
		this.type = type
		this.resize(w, h)
	}
	resize(w, h, fillValue=0) {
		this.width  = Math.max(0, w) || 0
		this.height = Math.max(0, h) || 0
		this.data   = new this.type(w * h)
		if (typeof fillValue == 'function') {
			this.fill(fillValue)
		} else {
			this.data.fill(fillValue)
		}
		return this
	}
	fillRect(tx, ty, tw, th, value) {
		for (let j=0; j<th; j++) {
			for (let i=0; i<tw; i++) {
				this.set(tx + i, ty + j, value)
			}
		}
		return this
	}
	fill(fn) {
		let idx = 0
		for (let j=0; j<this.height; j++) {
			for (let i=0; i<this.width; i++) {
				this.set(i, j, fn(i, j, idx++, this.width, this.height))
			}
		}
		return this
	}
	loop(fn) {
		let idx = 0
		for (let j=0; j<this.height; j++) {
			for (let i=0; i<this.width; i++) {
				fn(this.data[idx], i, j, idx++, this.width, this.height)
			}
		}
		return this
	}
	getMaxValue() {
		let m = 0
		this.data.forEach( v => m = Math.max(v, m))
		return m
	}
	isEqual(grid) {
		if (grid.width != this.width || grid.height != this.height) return false
		for (let i=0; i<this.data.length; i++) {
			if (this.data[i] !== grid.data[i]) return false
		}
		return true
	}
	getWrapped(gx, gy) {
		gx = (gx + this.width) % this.width
		gy = (gy + this.height) % this.height
		return this.data[gx + gy * this.width]
	}
	get(gx, gy) {
		if (gx < 0 || gy < 0 || gx >= this.width || gy >=this.height) return null
		return this.data[gx + gy * this.width]
	}
	sampleNearest(sx, sy) {
		const mirrorX = 1 - Math.abs(Math.abs(sx) % 2 - 1)
		const mirrorY = 1 - Math.abs(Math.abs(sy) % 2 - 1)
		const x = Math.round(mirrorX * (this.width-1))
		const y = Math.round(mirrorY * (this.height-1))
		return this.get(x, y)
  	}
	sample(sx, sy) {
		const x = sx * this.width - 0.5
		const y = sy * this.height - 0.5
	  	let l = Math.floor(x)
		let b = Math.floor(y)
		let r = l + 1
		let t = b + 1
		const lr = x - l
		const bt = y - b
		const p1 = mix(this.get(l, b), this.get(r, b), lr)
		const p2 = mix(this.get(l, t), this.get(r, t), lr)
		return mix(p1, p2, bt)
  	}
	getTransformed(flipX=0, flipY=0, rot=0) {
		let dw, dh
		if (rot == 1 || rot == 2) {
			dw = this.height
			dh = this.width
		} else {
			dw = this.width
			dh = this.height
		}
		const out = new Grid2(dw, dh, this.type)
		for (let j=0; j<out.height; j++){
			for (let i=0; i<out.width; i++){
				let ti, tj
				if (rot == 0) {
					ti = i
					tj = j
				} else if (rot == 1) {
					ti = out.height - j - 1
					tj = i
				} else if (rot == 2) {
					ti = out.width - i - 1
					tj = out.height - j - 1
				} else if (rot == 3) {
					ti = j
					tj = out.width - i - 1
				}
				ti = flipX ? out.width - 1 - ti : ti
				tj = flipY ? out.height - 1 - tj : tj
				out.set(ti, tj, this.get(i, j))
			}
		}
		return out
	}
	getRect(gx, gy, rw, rh) {
		const out = new Grid(rw, rh, this.type)
		for (let j=0; j<out.height; j++){
			for (let i=0; i<out.width; i++){
				out.set(i, j, this.get(i + gx, j + gy))
			}
		}
		return out
	}
	set(gx, gy, value) {
		if (gx < 0 || gy < 0 || gx >= this.width || gy >=this.height) return
		this.data[gx + gy * this.width] = value
	}
	setArray(array) {
		if (array.length != this.data.length) return
		array.forEach((e, idx) => this.data[idx] = e)
	}
	setRectArray(tx, ty, tw, th, array) {
		for (let j=0; j<th; j++) {
			for (let i=0; i<tw; i++) {
				this.set(tx + i, ty + j, array[i + tw * j])
			}
		}
	}
	setGrid(gx, gy, grid) {
		this.setRectArray(gx, gy, grid.width, grid.height, grid.data)
	}
	toString() {
		let out = ''
		for (let y = 0; y<this.height; y++) {
			for (let x = 0; x<this.width; x++) {
				out += this.get(x, y)
			}
			out += '\n'
		}
		return out
	}
	print() {
		console.log("%c" + this.toString(), "font-family: monospace;")
	}
}
function mix(v1, v2, a) {
	return v1 * (1 - a) + v2 * a
}
// SPDX-License-Identifier: Apache-2.0
// Derived from @thi-ng/umbrella random (arandom.ts)
class ARandom {
	INV_MAX = 1 / 2 ** 32
	int() {
	}
	float(norm = 1) {
		return this.int() * this.INV_MAX * norm
	}
	probability(p) {
		return this.float() < p
	}
	norm(norm = 1) {
		return (this.int() * this.INV_MAX - 0.5) * 2 * norm
	}
	normMinmax(min, max) {
		const x = this.minmax(min, max)
		return this.float() < 0.5 ? x : -x
	}
	minmax(min, max) {
		return this.float() * (max - min) + min
	}
	minmaxInt(min, max) {
		min |= 0
		const range = (max | 0) - min
		return range ? min + (this.int() % range) : min
	}
	minmaxUint(min, max) {
		min >>>= 0
		const range = (max >>> 0) - min
		return range ? min + (this.int() % range) : min
	}
	choose(){
		let arr
		if (arguments.length == 1 && Array.isArray(arguments[0])) {
			arr = arguments[0]
		} else {
			arr = [...arguments]
		}
		return arr[this.minmaxInt(0, arr.length)]
	}
	shuffleArray(arr) {
		for (let i = arr.length - 1; i > 0; i--) {
			const j = Math.floor(this.float() * (i + 1));
			[arr[i], arr[j]] = [arr[j], arr[i]]
		}
		return arr
	}
	chooseWeightedObj(mapWithWeightattribute) {
		const arrayOfArrays = Object.keys(mapWithWeightattribute).map( e => [mapWithWeightattribute[e].weight, e])
		const res = this.chooseWeighted(arrayOfArrays)
		return res
	}
	chooseWeighted(arrayOfArrays) {
		const weights = arrayOfArrays.map( e=> e[0])
		const arr = arrayOfArrays.map( e=> e[1])
		const cumulativeWeights = []
		for (let i = 0; i < weights.length; i++) {
			cumulativeWeights[i] = weights[i] + (cumulativeWeights[i - 1] || 0)
		}
		const maxCumulativeWeight = cumulativeWeights[cumulativeWeights.length - 1]
		const randomNumber = maxCumulativeWeight * this.float()
		for (let itemIndex = 0; itemIndex < arr.length; itemIndex++) {
			if (cumulativeWeights[itemIndex] >= randomNumber) {
				return arr[itemIndex]
			}
		}
	}
}
// SPDX-License-Identifier: Apache-2.0
// Derived from @thi-ng/umbrella random (sfc32.ts)
class SFC32 extends ARandom {
	constructor(seed) {
        super()
		this.buffer = new Uint32Array(4)
		this.seed(seed)
	}
	copy() {
		return new SFC32(this.buffer)
	}
	bytes() {
		return new Uint8Array(this.buffer.buffer)
	}
	int() {
		const s = this.buffer
		const t = (((s[0] + s[1]) >>> 0) + s[3]) >>> 0
		s[3] = (s[3] + 1) >>> 0
		s[0] = s[1] ^ (s[1] >>> 9)
		s[1] = (s[2] + (s[2] << 3)) >>> 0
		s[2] = (((s[2] << 21) | (s[2] >>> 11)) + t) >>> 0
		return t
	}
	seed(seed) {
		this.buffer.set(seed)
		return this
	}
}
function xfnv1a(str) {
	let h = 2166136261 >>> 0
	for (let i = 0; i < str.length; i++) {
		h = Math.imul(h ^ str.charCodeAt(i), 16777619)
	}
	return function() {
		h += h << 13
		h ^= h >>> 7
		h += h << 3
		h ^= h >>> 17
		return (h += h << 5) >>> 0
	}
}
function tokenizeLine(line) {
	const T = TokenTypes
	const out = new Array(line.length)
	for (let k = 0; k < line.length; k++) {
		out[k] = {
			char: line[k],
			type: T.DEFAULT
		}
	}
	const paint = (start, end, type) => {
		for (let k = start; k < end; k++) out[k].type = type
	}
	let i = 0
	while (i < line.length) {
		const c = line[i]
		if (c === "/" && line[i + 1] === "/") {
			paint(i, line.length, T.COMMENT)
			break
		}
		if (c === "/") {
			let k = i - 1
			while (k >= 0 && isWs(line[k])) k--
			const prevType = k >= 0 ? out[k].type : null
			const prevChar = k >= 0 ? line[k] : null
			const isDivision =
				prevType === T.IDENTIFIER ||
				prevType === T.NUMBER ||
				(prevType === T.PUNCTUATION && (prevChar === ")" || prevChar === "]"))
			if (!isDivision) {
				let j = i + 1
				let inClass = false
				while (j < line.length) {
					const ch = line[j]
					if (ch === "\\" && j + 1 < line.length) { j += 2; continue }
					if (ch === "[") { inClass = true; j++; continue }
					if (ch === "]") { inClass = false; j++; continue }
					if (ch === "/" && !inClass) { j++; break }
					j++
				}
				while (j < line.length && isAlpha(line[j])) j++
				paint(i, j, T.REGEX)
				i = j
				continue
			}
		}
		if (c === "\"" || c === "'" || c === "`") {
			const quote = c
			const type = (c === "`") ? T.TEMPLATE : T.STRING
			out[i++].type = type
			while (i < line.length) {
				out[i].type = type
				if (line[i] === "\\" && i + 1 < line.length) {
					out[i + 1].type = type
					i += 2
					continue
				}
				if (line[i] === quote) {
					i++
					break
				}
				i++
			}
			continue
		}
		if (isDigit(c) || (c === "." && isDigit(line[i + 1]))) {
			let j = i
			if (c === "0" && (line[i + 1] === "x" || line[i + 1] === "X")) {
				j += 2
				while (j < line.length && isHex(line[j])) j++
			} else {
				while (j < line.length && isDigit(line[j])) j++
				if (line[j] === ".") { j++; while (j < line.length && isDigit(line[j])) j++}
				if (line[j] === "e" || line[j] === "E") {
					j++
					if (line[j] === "+" || line[j] === "-") j++
					while (j < line.length && isDigit(line[j])) j++
				}
			}
			paint(i, j, T.NUMBER)
			i = j
			continue
		}
		if (isAlpha(c) || (c === "#" && isAlpha(line[i + 1]))) {
			let j = i + 1
			while (j < line.length && isAlphaNum(line[j])) j++
			const word = line.slice(i, j)
			paint(i, j, JS_KEYWORDS.has(word) ? T.KEYWORD : T.IDENTIFIER)
			i = j
			continue
		}
		if (isOperator(c)) {
			out[i++].type = T.OPERATOR
			continue
		}
		if (isPunct(c)) {
			out[i++].type = T.PUNCTUATION
			continue
		}
		if (isWs(c)) {
			out[i++].type = T.WHITESPACE
			continue
		}
		console.log("Default token found: \n" + c)
		console.log(line)
		i++
	}
	return out
}
const TokenTypes = {
	WHITESPACE:  Symbol("WHITESPACE"),
	KEYWORD:     Symbol("KEYWORD"),
	IDENTIFIER:  Symbol("IDENTIFIER"),
	NUMBER:      Symbol("NUMBER"),
	STRING:      Symbol("STRING"),
	TEMPLATE:    Symbol("TEMPLATE"),
	OPERATOR:    Symbol("OPERATOR"),
	PUNCTUATION: Symbol("PUNCTUATION"),
	REGEX:       Symbol("REGEX"),
	COMMENT:     Symbol("COMMENT"),
	DEFAULT:     Symbol("DEFAULT"),
}
const JS_KEYWORDS = new Set([
	"abstract",
	"arguments ",
	"as ",
	"async",
	"await",
	"boolean",
	"break",
	"byte",
	"case",
	"catch",
	"char",
	"class",
	"const",
	"continue",
	"debugger",
	"default",
	"delete",
	"do",
	"double",
	"else",
	"enum",
	"eval ",
	"export",
	"extends",
	"false",
	"final",
	"finally",
	"float",
	"for",
	"from ",
	"function",
	"get",
	"goto",
	"if",
	"implements",
	"import",
	"in",
	"instanceof",
	"int",
	"interface",
	"let ",
	"long",
	"native",
	"new",
	"null",
	"of",
	"package",
	"private",
	"protected",
	"public",
	"return",
	"set",
	"short",
	"static",
	"super",
	"switch",
	"synchronized",
	"this",
	"throw",
	"throws",
	"transient",
	"true",
	"try",
	"typeof",
	"var",
	"void",
	"volatile",
	"while",
	"with",
	"yield",
])
const isDigit    = c => c >= "0" && c <= "9"
const isHex      = c => isDigit(c) || (c >= "a" && c <= "f") || (c >= "A" && c <= "F")
const isAlpha    = c => (c >= "a" && c <= "z") || (c >= "A" && c <= "Z") || c === "_" || c === "$"
const isAlphaNum = c => isAlpha(c) || isDigit(c)
const isOperator = c => "+-*/%=<>!&|^~?:".includes(c)
const isPunct    = c => "()[]{},;.".includes(c)
const isWs       = c => c === " " || c === "\t"
function syntaxHighlight(lines) {
	return lines.map(tokenizeLine)
}
function buildPerm(rnd) {
	const p = new Uint8Array(512)
	for (let i = 0; i < 256; i++) p[i] = i
	for (let i = 255; i >= 0; i--) {
		const j = rnd.minmaxInt(0, i + 1)
		const t = p[i]
		p[i] = p[j]
		p[j] = t
		p[256 + i] = p[i]
	}
	return p
}
function smooth5(t) {
	return t * t * t * (t * (t * 6 - 15) + 10)
}
function valueNoise(rnd) {
	const perm = buildPerm(rnd.copy())
	function lattice2(ix, iy) {
		const i = (perm[ix & 0xff] + (iy & 0xff)) & 0xff
		return perm[i] / 255
	}
	function lattice3(ix, iy, iz) {
		const a = perm[ix & 0xff] + (iy & 0xff)
		const b = perm[a & 0xff] + (iz & 0xff)
		return perm[b & 0xff] / 255
	}
	function noise2D(x, y) {
		const ix = Math.floor(x)
		const iy = Math.floor(y)
		const fx = x - ix
		const fy = y - iy
		const tx = smooth5(fx)
		const ty = smooth5(fy)
		const v00 = lattice2(ix, iy)
		const v10 = lattice2(ix + 1, iy)
		const v01 = lattice2(ix, iy + 1)
		const v11 = lattice2(ix + 1, iy + 1)
		const v0 = v00 + tx * (v10 - v00)
		const v1 = v01 + tx * (v11 - v01)
		return v0 + ty * (v1 - v0)
	}
	function noise3D(x, y, z) {
		const ix = Math.floor(x)
		const iy = Math.floor(y)
		const iz = Math.floor(z)
		const fx = x - ix
		const fy = y - iy
		const fz = z - iz
		const tx = smooth5(fx)
		const ty = smooth5(fy)
		const tz = smooth5(fz)
		const v000 = lattice3(ix, iy, iz)
		const v100 = lattice3(ix + 1, iy, iz)
		const v010 = lattice3(ix, iy + 1, iz)
		const v110 = lattice3(ix + 1, iy + 1, iz)
		const v001 = lattice3(ix, iy, iz + 1)
		const v101 = lattice3(ix + 1, iy, iz + 1)
		const v011 = lattice3(ix, iy + 1, iz + 1)
		const v111 = lattice3(ix + 1, iy + 1, iz + 1)
		const v00 = v000 + tx * (v100 - v000)
		const v10 = v010 + tx * (v110 - v010)
		const v01 = v001 + tx * (v101 - v001)
		const v11 = v011 + tx * (v111 - v011)
		const v0 = v00 + ty * (v10 - v00)
		const v1 = v01 + ty * (v11 - v01)
		return v0 + tz * (v1 - v0)
	}
	function fractal2D(x, y, opts = {}) {
		const octaves = opts.octaves ?? 4
		const lacunarity = opts.lacunarity ?? 2
		const persistence = opts.persistence ?? 0.5
		let sum = 0
		let amp = 1
		let freq = 1
		let maxVal = 0
		for (let o = 0; o < octaves; o++) {
			sum += amp * noise2D(x * freq, y * freq)
			maxVal += amp
			amp *= persistence
			freq *= lacunarity
		}
		return sum / maxVal
	}
	function fractal3D(x, y, z, opts = {}) {
		const octaves = opts.octaves ?? 4
		const lacunarity = opts.lacunarity ?? 2
		const persistence = opts.persistence ?? 0.5
		let sum = 0
		let amp = 1
		let freq = 1
		let maxVal = 0
		for (let o = 0; o < octaves; o++) {
			sum += amp * noise3D(x * freq, y * freq, z * freq)
			maxVal += amp
			amp *= persistence
			freq *= lacunarity
		}
		return sum / maxVal
	}
	return {
		noise2D,
		noise3D,
		fractal2D,
		fractal3D
	}
}
const PALETTES = [
	{
		name : "MONO",
		bg : 0x000,
		tokens : {
			[TokenTypes.DEFAULT] : 0x999,
		},
	},
	{
		name : "P1",
		bg : 0x000,
		tokens : {
			[TokenTypes.DEFAULT] : 0x3C3,
		},
	},
	{
		name : "P3",
		bg : 0x210,
		tokens : {
			[TokenTypes.DEFAULT] : 0xF90,
		},
	},
	{
		name : "PINK",
		bg : 0x000,
		tokens : {
			[TokenTypes.DEFAULT] : 0x999,
			[TokenTypes.KEYWORD] : 0xF3A,
		},
	},
	{
		name : "BSOD",
		bg : 0x00A,
		tokens : {
			[TokenTypes.DEFAULT] : 0xAAA,
			[TokenTypes.KEYWORD] : 0xFFF,
		},
	},
	{
		name : "POMC",
		bg : 0x000,
		tokens : {
			[TokenTypes.DEFAULT] : 0x999,
			[TokenTypes.KEYWORD]     : 0x00A,
			[TokenTypes.IDENTIFIER]  : 0x0A0,
			[TokenTypes.NUMBER]      : 0x0AA,
			[TokenTypes.STRING]      : 0xA00,
			[TokenTypes.COMMENT]     : 0xA0A,
			[TokenTypes.OPERATOR]    : 0xA50,
			[TokenTypes.PUNCTUATION] : 0xAAA,
		},
	},
	{
		name : "COMP",
		bg : 0x000,
		tokens : {
			[TokenTypes.DEFAULT]     : 0xAAA,
			[TokenTypes.KEYWORD]     : 0x55F,
			[TokenTypes.IDENTIFIER]  : 0x5F5,
			[TokenTypes.NUMBER]      : 0x5FF,
			[TokenTypes.STRING]      : 0xF55,
			[TokenTypes.COMMENT]     : 0xF5F,
			[TokenTypes.OPERATOR]    : 0xFF5,
			[TokenTypes.PUNCTUATION] : 0xFFF,
		},
	},
	{
		name : "AGE",
		bg : 0x000,
		tokens : {
			[TokenTypes.DEFAULT]     : 0xA50,
			[TokenTypes.KEYWORD]     : 0xAAA,
			[TokenTypes.IDENTIFIER]  : 0xF55,
			[TokenTypes.NUMBER]      : 0xFF5,
			[TokenTypes.STRING]      : 0xF5F,
			[TokenTypes.COMMENT]     : 0x5FF,
			[TokenTypes.OPERATOR]    : 0x0A0,
			[TokenTypes.PUNCTUATION] : 0x0AA,
		},
	},
	{
		name : "HELP",
		bg : 0x0AA,
		tokens : {
			[TokenTypes.DEFAULT]     : 0x000,
			[TokenTypes.KEYWORD]     : 0x008,
			[TokenTypes.IDENTIFIER]  : 0x000,
			[TokenTypes.NUMBER]      : 0xA00,
			[TokenTypes.STRING]      : 0xA00,
			[TokenTypes.COMMENT]     : 0x555,
			[TokenTypes.OPERATOR]    : 0x008,
			[TokenTypes.PUNCTUATION] : 0x000,
		},
	},
	{
		name : "TURBO",
		bg : 0x00A,
		tokens : {
			[TokenTypes.DEFAULT]     : 0xAAA,
			[TokenTypes.KEYWORD]     : 0xFFF,
			[TokenTypes.IDENTIFIER]  : 0x55F,
			[TokenTypes.NUMBER]      : 0xFF5,
			[TokenTypes.STRING]      : 0x5FF,
			[TokenTypes.COMMENT]     : 0x5F5,
			[TokenTypes.OPERATOR]    : 0xF55,
			[TokenTypes.PUNCTUATION] : 0xF5F,
		},
	},
	{
		name : "NC",
		bg : 0x00A,
		tokens : {
			[TokenTypes.DEFAULT]     : 0xAAA,
			[TokenTypes.KEYWORD]     : 0x5F5,
			[TokenTypes.IDENTIFIER]  : 0xFFF,
			[TokenTypes.NUMBER]      : 0xFF5,
			[TokenTypes.STRING]      : 0xF55,
			[TokenTypes.COMMENT]     : 0x55F,
			[TokenTypes.OPERATOR]    : 0xF5F,
			[TokenTypes.PUNCTUATION] : 0x5FF,
		},
	},
	{
		name : "BURN",
		bg : 0x000,
		tokens : {
			[TokenTypes.DEFAULT]     : 0xDDC,
			[TokenTypes.KEYWORD]     : 0xFDA,
			[TokenTypes.IDENTIFIER]  : 0xCDC,
			[TokenTypes.NUMBER]      : 0xDC9,
			[TokenTypes.STRING]      : 0xC99,
			[TokenTypes.COMMENT]     : 0x7A7,
			[TokenTypes.OPERATOR]    : 0xFED,
			[TokenTypes.PUNCTUATION] : 0xCCB,
		},
	},
	{
		name : "KAI",
		bg : 0x000,
		tokens : {
			[TokenTypes.DEFAULT]     : 0xFFE,
			[TokenTypes.KEYWORD]     : 0xF38,
			[TokenTypes.IDENTIFIER]  : 0xAEF,
			[TokenTypes.NUMBER]      : 0xA8F,
			[TokenTypes.STRING]      : 0xEE7,
			[TokenTypes.COMMENT]     : 0x764,
			[TokenTypes.OPERATOR]    : 0xF38,
			[TokenTypes.PUNCTUATION] : 0xFFE,
		},
	},
	{
		name : "SOL",
		bg : 0x222,
		tokens : {
			[TokenTypes.DEFAULT]     : 0x896,
			[TokenTypes.KEYWORD]     : 0x6A0,
			[TokenTypes.IDENTIFIER]  : 0x9AA,
			[TokenTypes.NUMBER]      : 0xC2A,
			[TokenTypes.STRING]      : 0x2BB,
			[TokenTypes.COMMENT]     : 0x466,
			[TokenTypes.OPERATOR]    : 0xC60,
			[TokenTypes.PUNCTUATION] : 0x896,
		},
	},
	{
		name : "GRU",
		bg : 0x222,
		tokens : {
			[TokenTypes.DEFAULT]     : 0xEDB,
			[TokenTypes.KEYWORD]     : 0xF53,
			[TokenTypes.IDENTIFIER]  : 0xAB9,
			[TokenTypes.NUMBER]      : 0xD8A,
			[TokenTypes.STRING]      : 0xBC2,
			[TokenTypes.COMMENT]     : 0x886,
			[TokenTypes.OPERATOR]    : 0xF82,
			[TokenTypes.PUNCTUATION] : 0xEDB,
		},
	},
	{
		name : "WAVE",
		bg : 0x103,
		tokens : {
			[TokenTypes.DEFAULT]     : 0xFFF,
			[TokenTypes.KEYWORD]     : 0xF3C,
			[TokenTypes.IDENTIFIER]  : 0x9DF,
			[TokenTypes.NUMBER]      : 0xFC3,
			[TokenTypes.STRING]      : 0x3FC,
			[TokenTypes.COMMENT]     : 0x636,
			[TokenTypes.OPERATOR]    : 0xF3F,
			[TokenTypes.PUNCTUATION] : 0xCCF,
		},
	},
]
const WarpFunctions = {
	plasma : (rnd, noise) => {
		function dot(ax, ay, bx, by) {
			return ax * bx + ay * by
		}
		const scl = rnd.minmax(0.5, 3.5)
		const l = rnd.minmax(1, 4)
		const f1 = rnd.minmax(0.005, 0.05) * rnd.choose(-1, 1)
		const f2 = rnd.minmax(0.005, 0.05) * rnd.choose(-1, 1)
		const f3 = rnd.minmax(0.005, 0.05) * rnd.choose(-1, 1)
		const f4 = rnd.minmax(0.005, 0.05) * rnd.choose(-1, 1)
		const f5 = rnd.minmax(0.005, 0.05) * rnd.choose(-1, 1)
		return {
			fn : (s, t, i, j, frame) => {
				const r = Math.sqrt(s * s + t * t) * l
				const S = Math.sin(frame * f1) * scl
				const C = Math.cos(frame * f2) * scl
				const v1 = Math.sin(dot(s, t, S, C))
				const v2 = Math.cos(r + frame * f3)
				const v3 = Math.sin(s * scl + frame * f4)
				const v4 = Math.cos(t * scl + frame * f5)
				const sum = v1 + v2 + v3 + v4
				const n = Math.max(0, Math.min(1, (sum + 4) / 8))
				const bin = Math.min(8, Math.floor(n * 9))
				return bin
			}
		}
	},
	valueNoise : (rnd, noise) => {
		const cr = rnd.minmax(0.003, 0.02) * rnd.choose(-1, 1)
		const sr = rnd.minmax(0.003, 0.02) * rnd.choose(-1, 1)
		const zr = rnd.minmax(0.001, 0.005) * rnd.choose(-1, 1)
		const scl = rnd.minmax(0.5, 2.0)
		return {
			fn : (s, t, i, j, frame) => {
				const ox = Math.cos(frame * cr)
				const oy = Math.sin(frame * sr)
				const z  = Math.sin(frame * zr)
				const n = Math.max(0, Math.min(1, noise(s * scl + ox, t * scl + oy, z)))
				const bin = Math.min(7, Math.floor(n * 8))
				return bin + 1
			}
		}
	},
	alternate : (rnd, noise) => {
		const line_width = rnd.minmaxInt(1, 16)
		const start = rnd.choose(0, 1)
		return {
			fn : rnd.chooseWeighted([
				[4,
					(s, t, i, j, frame) => {
						const offs = Math.floor(j / line_width + start) % 2
						return 3 + 4 * offs
					}
				],
				[1,
					(s, t, i, j, frame) => {
						const offs = Math.floor(i / line_width + start) % 2
						return 1 + 4 * offs
					}
				]
			])
		}
	},
}
function emitTexturedBox(index, buf, uvBuf, x, y, w, h) {
	let offsetP = index * 6 * 2
	let offsetU = index * 6 * 2
	const a0x = x
	const a0y = y
	const a1x = x + w
	const a1y = y
	const a2x = x + w
	const a2y = y + h
	const a3x = x
	const a3y = y + h
	buf[offsetP++] = a0x
	buf[offsetP++] = a0y
	buf[offsetP++] = a1x
	buf[offsetP++] = a1y
	buf[offsetP++] = a3x
	buf[offsetP++] = a3y
	buf[offsetP++] = a3x
	buf[offsetP++] = a3y
	buf[offsetP++] = a1x
	buf[offsetP++] = a1y
	buf[offsetP++] = a2x
	buf[offsetP++] = a2y
	if (uvBuf !== null) {
		uvBuf[offsetU++] = 0
		uvBuf[offsetU++] = 0
		uvBuf[offsetU++] = 1
		uvBuf[offsetU++] = 0
		uvBuf[offsetU++] = 0
		uvBuf[offsetU++] = 1
		uvBuf[offsetU++] = 0
		uvBuf[offsetU++] = 1
		uvBuf[offsetU++] = 1
		uvBuf[offsetU++] = 0
		uvBuf[offsetU++] = 1
		uvBuf[offsetU++] = 1
	}
}
function emitOffset(index, buf, intValue) {
	let offset = index * 2 * 3
	for (let i=0; i<6; i++){
        buf[offset++] = intValue
    }
}
function emitColors(index, buf, rgbArray ) {
	let offset = index * 6 * 3
	for (let i=0; i<6; i++){
		buf[offset++] = rgbArray[0]
		buf[offset++] = rgbArray[1]
		buf[offset++] = rgbArray[2]
	}
}
class Buffer {
	constructor(gl, attributeName, numComponents, numElements) {
		this.gl = gl
		this.attributeName = attributeName
		this.buffer = gl.createBuffer()
		this.numComponents = numComponents
		this.numElements = numElements
		this.data = new Float32Array(numElements * numComponents)
	}
	enable(program) {
		const gl = this.gl
		const loc = gl.getAttribLocation(program, this.attributeName)
		gl.bindBuffer(gl.ARRAY_BUFFER, this.buffer)
		gl.enableVertexAttribArray(loc)
		gl.vertexAttribPointer(loc, this.numComponents, this.gl.FLOAT, false, 0, 0)
	}
	bufferData(mode = this.gl.DYNAMIC_DRAW) {
		const gl = this.gl
		gl.bindBuffer(gl.ARRAY_BUFFER, this.buffer)
		gl.bufferData(gl.ARRAY_BUFFER, this.data, mode)
	}
}
function compileShader(gl, source, type) {
	const shader = gl.createShader(type)
	gl.shaderSource(shader, source)
	gl.compileShader(shader)
	if (gl.getShaderParameter(shader, gl.COMPILE_STATUS) !== true){
		throw gl.getShaderInfoLog(shader)
	}
	return shader
}
function createProgram(gl, vSource, fSource) {
	const vShader = compileShader(gl, vSource, gl.VERTEX_SHADER)
	const fShader = compileShader(gl, fSource, gl.FRAGMENT_SHADER)
	const program = gl.createProgram()
	gl.attachShader(program, vShader)
	gl.attachShader(program, fShader)
	gl.linkProgram(program)
	if (gl.getProgramParameter(program, gl.LINK_STATUS) !== true)
		throw gl.getProgramInfoLog(program)
	return program
}
// Program wrapper derived from Plask MagicProgram (c) Dean McNamee, MIT
class Program {
	gl
  	program
  	uniforms
	constructor(gl, program){
  		this.gl = gl
  		this.program = program
  		this.uniforms = []
  		this.locations = []
		const num_uniforms = gl.getProgramParameter(program, gl.ACTIVE_UNIFORMS)
		for (let i = 0; i < num_uniforms; ++i) {
			const info = gl.getActiveUniform(program, i)
			const name = info.name
			const loc = gl.getUniformLocation(program, name)
			this.uniforms[name] = this.#makeSetter(info.type, loc, info.size)
			this.locations[name] = loc
		}
		const num_attribs = gl.getProgramParameter(program, gl.ACTIVE_ATTRIBUTES)
		for (let i = 0; i < num_attribs; ++i) {
			const info = gl.getActiveAttrib(program, i)
			const name = info.name
			const loc = gl.getAttribLocation(program, name)
			this.locations[name] = loc
		}
  	}
  	set(uniformName, ...args) {
  		if (!this.uniforms[uniformName]) ; else {
  			this.uniforms[uniformName](...args)
  		}
  	}
  	#makeSetter(type, loc, size = 1) {
  		const gl = this.gl
		const matData = value => value && value.toFloat32Array ? value.toFloat32Array() : value
		const isArrayLike = value => Array.isArray(value) || ArrayBuffer.isView(value)
		switch (type) {
			case gl.BOOL:
			case gl.INT:
			case gl.SAMPLER_2D:
			case gl.SAMPLER_2D_RECT:
			case gl.SAMPLER_CUBE:
			case gl.SAMPLER_3D:
			case gl.SAMPLER_2D_SHADOW:
			case gl.SAMPLER_2D_ARRAY:
			case gl.SAMPLER_2D_ARRAY_SHADOW:
			case gl.SAMPLER_CUBE_SHADOW:
			case gl.INT_SAMPLER_2D:
			case gl.INT_SAMPLER_3D:
			case gl.INT_SAMPLER_CUBE:
			case gl.INT_SAMPLER_2D_ARRAY:
			case gl.UNSIGNED_INT_SAMPLER_2D:
			case gl.UNSIGNED_INT_SAMPLER_3D:
			case gl.UNSIGNED_INT_SAMPLER_CUBE:
			case gl.UNSIGNED_INT_SAMPLER_2D_ARRAY:
				return function(value) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(value))) gl.uniform1iv(loc, value)
					else gl.uniform1i(loc, value)
					return this
				}
			case gl.UINT:
				return function(value) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(value))) gl.uniform1uiv(loc, value)
					else gl.uniform1ui(loc, value)
					return this
				}
			case gl.FLOAT:
				return function(value) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(value))) gl.uniform1fv(loc, value)
					else gl.uniform1f(loc, value)
					return this
				}
			case gl.BOOL_VEC2:
			case gl.INT_VEC2:
				return function(x, y) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(x))) gl.uniform2iv(loc, x)
					else gl.uniform2i(loc, x, y)
					return this
				}
			case gl.BOOL_VEC3:
			case gl.INT_VEC3:
				return function(x, y, z) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(x))) gl.uniform3iv(loc, x)
					else gl.uniform3i(loc, x, y, z)
					return this
				}
			case gl.BOOL_VEC4:
			case gl.INT_VEC4:
				return function(x, y, z, w) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(x))) gl.uniform4iv(loc, x)
					else gl.uniform4i(loc, x, y, z, w)
					return this
				}
			case gl.UINT_VEC2:
				return function(x, y) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(x))) gl.uniform2uiv(loc, x)
					else gl.uniform2ui(loc, x, y)
					return this
				}
			case gl.UINT_VEC3:
				return function(x, y, z) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(x))) gl.uniform3uiv(loc, x)
					else gl.uniform3ui(loc, x, y, z)
					return this
				}
			case gl.UINT_VEC4:
				return function(x, y, z, w) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(x))) gl.uniform4uiv(loc, x)
					else gl.uniform4ui(loc, x, y, z, w)
					return this
				}
			case gl.FLOAT_VEC2:
				return function(x, y) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(x))) gl.uniform2fv(loc, x)
					else gl.uniform2f(loc, x, y)
					return this
				}
			case gl.FLOAT_VEC3:
				return function(x, y, z) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(x))) gl.uniform3fv(loc, x)
					else gl.uniform3f(loc, x, y, z)
					return this
				}
			case gl.FLOAT_VEC4:
				return function(x, y, z, w) {
					if (arguments.length === 1 && (size > 1 || isArrayLike(x))) gl.uniform4fv(loc, x)
					else gl.uniform4f(loc, x, y, z, w)
					return this
				}
			case gl.FLOAT_MAT2:
				return function(mat2) {
					gl.uniformMatrix2fv(loc, false, matData(mat2))
					return this
				}
			case gl.FLOAT_MAT3:
				return function(mat3) {
					gl.uniformMatrix3fv(loc, false, matData(mat3))
					return this
				}
			case gl.FLOAT_MAT4:
				return function(mat4) {
					gl.uniformMatrix4fv(loc, false, matData(mat4))
					return this
				}
			case gl.FLOAT_MAT2x3:
				return function(mat) {
					gl.uniformMatrix2x3fv(loc, false, matData(mat))
					return this
				}
			case gl.FLOAT_MAT2x4:
				return function(mat) {
					gl.uniformMatrix2x4fv(loc, false, matData(mat))
					return this
				}
			case gl.FLOAT_MAT3x2:
				return function(mat) {
					gl.uniformMatrix3x2fv(loc, false, matData(mat))
					return this
				}
			case gl.FLOAT_MAT3x4:
				return function(mat) {
					gl.uniformMatrix3x4fv(loc, false, matData(mat))
					return this
				}
			case gl.FLOAT_MAT4x2:
				return function(mat) {
					gl.uniformMatrix4x2fv(loc, false, matData(mat))
					return this
				}
			case gl.FLOAT_MAT4x3:
				return function(mat) {
					gl.uniformMatrix4x3fv(loc, false, matData(mat))
					return this
				}
		}
		return function() {
			throw "Unknown uniform type: " + type
		}
	}
	use() {
  		this.gl.useProgram(this.program)
	}
	relink() {
		const gl = this.gl
		const program = this.program
		gl.linkProgram(program)
		if (gl.getProgramParameter(program, gl.LINK_STATUS) !== true)
		throw gl.getProgramInfoLog(program)
		return true
	}
}
function decodeData(dataStringB64) {
	return new Uint8Array(atob(dataStringB64).split('').map(el => el.charCodeAt(0)))
}
function getBitArrayFromData(dataStringB64, numBits) {
	const bytes = decodeData(dataStringB64)
	const bits = new Uint8Array(numBits)
	for (let i=0; i<numBits; i++) {
		const bitIdx = Math.floor(i % 8)
		const byteIdx = Math.floor(i / 8)
		bits[i] = bit_get(bytes[byteIdx], bitIdx)
	}
	return bits
}
const bit_get = (n, bit) => (n>>bit) % 2
const CHAR_SHADER = {
	vert : [
		"#version 300 es",
		"precision highp float;",
		"uniform vec2 u_resolution;",
		"in float a_fontOffset;",
		"in vec3 a_charFgColor;",
		"in vec3 a_charBgColor;",
		"in vec2 a_position;",
		"in vec2 a_uv;",
		"uniform vec2 u_fontTableSize;",
		"out vec4 fgColor;",
		"out vec4 bgColor;",
		"out vec2 st;",
		"vec2 fontOffset;",
		"void main() {",
		"    vec4 screenTransform = vec4(2.0 / u_resolution.x, -2.0 / u_resolution.y, -1.0, 1.0);",
		"    gl_Position = vec4(a_position.xy * screenTransform.xy + screenTransform.zw, 0.0, 1.0);",
		"    fgColor = vec4(a_charFgColor, 1.0);",
		"    bgColor = vec4(a_charBgColor, 1.0);",
		"    float cols = u_fontTableSize.x;",
		"    fontOffset.x = mod(a_fontOffset, cols);",
		"    fontOffset.y = floor(a_fontOffset / cols);",
		"    st = (a_uv + fontOffset) / u_fontTableSize;",
		"}",
	].join("\n"),
	frag : [
		"#version 300 es",
		"precision highp float;",
		"uniform sampler2D u_texture;",
		"in vec4 bgColor;",
		"in vec4 fgColor;",
		"in vec2 st;",
		"out vec4 color;",
		"void main() {",
		"    color = mix(bgColor, fgColor, texture(u_texture, st).r);",
		"}",
	].join("\n"),
}
const fontData = {
	fontW : 8,
	fontH : 16,
	numChars : 96,
	cols : 8,
	rows : 12,
	charSet : " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~⌂",
	data : [
		'AAAAABgAAAAAAGYAGAAADAAYZgA+ABwMADxmNmMANgwAPCQ2Q0M2BgA8AH8DYxwAABgANj4wbgA',
		'AGAA2YBg7AAAYADZgDDMAAAAAf2EGMwAAGAA2Y2MzAAAYADY+YW4AAAAAABgAAAAAAAAAGAAAAA',
		'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAMAwAAAAAAAAYGAAAAAAAAAwwAAAAAABAD',
		'DBmGAAAAGAMMDwYAAAAMAww/34AfwAYDDA8GAAAAAwMMGYYGAAABhgYAAAYABgDMAwAABgAGAEA',
		'AAAADAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAcGD4+MH8cfzY',
		'cY2M4AwZjYx5gYDwDA2BjGDBgNgMDYGsYGDwzPz8waxgMYH9gYxhjGAZgMGBjDGMYA2AwYGMMNh',
		'hjYzBjYwwcfn8+eD4+DAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA',
		'AAAAAAAAD4+AAAAAAA+Y2MAAGAABmNjYxgYMAAMY2NjGBgYfhgwPn4AAAwAMBhjYAAABgBgGGNg',
		'AAAMfjAYY2AYGBgAGABjMBgYMAAMGD4eAAxgAAYYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA',
		'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAg/PB9/fzw+HGZmNmZmZmM2ZkNmRkZDY2NmA2YWFgN7Yz',
		'4DZh4eA3t/ZgNmFhZ7e2NmA2YGBmM7Y2ZDZkYGYwNjZmY2ZgZmPmM/PB9/D1wAAAAAAAAAAAAAA',
		'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABjPHhnD2NjPmMYMGYGd2djYxgw',
		'ZgZ/b2NjGDA2Bn9/Y38YMB4Ga3tjYxgwHgZjc2NjGDM2BmNjY2MYM2ZGY2NjYxgzZmZjY2NjPB5',
		'nf2NjPgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD8+Pz',
		'5+Y2NjZmNmY35jY2NmY2ZjWmNjY2ZjZgYYY2NjPmM+HBhjY2sGYzYwGGNjawZjZmAYY2NrBmtmY',
		'xhjNn8Ge2ZjGGMcdw8+Zz48Pgg2ADAAAAAAAAAAcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA',
		'AAAIAAAAAAAAABwAY2Z/PAA8NgBjZmMMATBjADZmYQwDMAAAPmYwDAcwAAAcPBgMDjAAABwYDAw',
		'cMAAAPhgGDDgwAAA2GEMMcDAAAGMYYwxgMAAAYzx/PEA8AAAAAAAAAAAAAAAAAAAAAAD/AAAAAA',
		'AAAAAAAAAAAAAAAAwAAAAAAAAADAAAAAAAAAAYAAcAOAAcAAAABgAwADYAAAAGADAAJgAAHh4+P',
		'D4GbgAwNmM2Yw8zAD5mAzN/BjMAM2YDMwMGMwAzZgMzAwYzADNmYzNjBjMAbj4+bj4PPgAAAAAA',
		'AAAwAAAAAAAAADMAAAAAAAAAHgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAcYYAccAAAABhhgBhg',
		'AAAAGAAAGGAAAADYccGYYNzs+bhhgNhh/ZmNmGGAeGGtmY2YYYB4Ya2ZjZhhgNhhrZmNmGGBmGG',
		'tmY2c8YGc8Y2Y+AABmAAAAAAAAAGYAAAAAAAAAPAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA',
		'AAAAAAAAAgAAAAAAAAADAAAAAAAAAAMAAAAO247Pj8zZmNmM25jDDNmY2YzZgYMM2ZrZjMGHAwz',
		'ZmtmMwYwDDNma2YzBmNsMzx/Pj4PPjhuGDYGMAAAAAAAAAYwAAAAAAAAD3gAAAAAAAAAAAAAAAA',
		'AAAAAAAAAAAAAAAAAAAAAAAAAAABwGA5uAAAAABgYGDsAAAAAGBgYAABjY38YGBgAADZjMw4YcA',
		'AAHGMYGBgYAAAcYwwYGBgAABxjBhgYGAAANmNjGBgYAABjfn9wGA4AAABgAAAAAAAAADAAAAAAA',
		'AAAHwAAAAAAAAAAAAAAAAAA'
	]
};
// Meltdown v1.2, ertdfgcvb.xyz
async function boot(canvas, settings) {
	const PROJECT_NAME = "Meltdown"
	const LOCATIONS = [
		"v1.0: Lugano, 2023",
		"v1.1: Tokyo, February 2026",
		"v1.2: Basel, June 2026",
	].join("\n")
	const seed = xfnv1a(settings.seed)
	const RND = new SFC32([seed(), seed(), seed(), seed()])
	const NOISE = valueNoise(RND).fractal3D
	console.log([PROJECT_NAME, LOCATIONS].join("\n"))
	console.log("Booting with settings: \n" + JSON.stringify(settings, null, 4))
	const MAX_CHARS = settings.maxChars
	const CHAR_LOOKUP = {}
	fontData.charSet.split("").forEach((v, idx) => CHAR_LOOKUP[v] = idx)
	const SPACE = CHAR_LOOKUP[" "]
	const gl = canvas.getContext("webgl2" )
	if (!gl) throw new Error("WebGL error, program halted.")
	canvas.style.imageRendering = "pixelated"
	canvas.style.touchAction = "none"
	const textProgram = new Program(gl, createProgram(gl, CHAR_SHADER.vert, CHAR_SHADER.frag))
	const tokens = await (async () => {
		let src
		const candidates = [
			new URL("../build/js.js", import.meta.url).href,
			new URL("js.js", import.meta.url).href,
		]
		for (const url of candidates) {
			try {
				const res = await fetch(url)
				if (!res.ok) throw new Error(res.status)
				src = await res.text()
				break
			} catch {}
		}
		if (!src) src = document.getElementById('meltdown').textContent
		const lines = src.split("\n")
			.filter(line => line.trim().length > 0)
			.map(line => {
				const LEADING_CHAR = settings.padSpace ? " " : "\t"
				const ENDING_CHAR = settings.padSpace ? " " : ""
				const n = line.match(/^\t*/)[0].length
				const leading = n === 0 ? LEADING_CHAR : "\t".repeat(4 * n - 1) + LEADING_CHAR
				return leading + line.slice(n).trimEnd() + ENDING_CHAR
			})
		const uniqueChars = getUniqueChars(src).filter(ch => ch !== "\n" && ch !== "\t" && ch !== " " && ch !== "\r").sort()
		uniqueChars.forEach(ch => {
			if (!fontData.charSet.includes(ch)) {
				console.warn("Missing char: " + ch + " (" + ch.charCodeAt(0) + ")")
			}
		})
		return syntaxHighlight(lines)
	})()
	{
		const counts = new Map()
		for (const row of tokens) {
			for (const tok of row) counts.set(tok.type, (counts.get(tok.type) || 0) + 1)
		}
		const total = [...counts.values()].reduce((a, b) => a + b, 0)
		const table = [
			{ name: "Lines of code",  count: tokens.length },
			{ name: "Tokens", count: total },
			...Object.entries(TokenTypes).map(([name, sym]) => ({ name, count: counts.get(sym) || 0 })),
		]
		console.table(table, ["name", "count"])
	}
	const lineSampler = (function (tokens) {
		const numRows = tokens.length
		const numCols = Math.max(...tokens.map(line => line.length))
		return {
			sampleToken : function(u, v) {
				if (u < 0 || u > 1 || v < 0 || v > 1) return null
				const i = Math.floor(u * numCols)
				const j = Math.floor(v * numRows)
				return this.getChar(i, j)
			},
			getLine : function(j) {
				if (j < 0 || j >= numRows) return null
				return tokens[j]
			},
			getToken : function(i, j) {
				if (i < 0 || i >= numCols || j < 0 || j >= numRows) return null
				const row = tokens[j]
				if (!row) return null
				return row[i] || null
			},
			cols : numCols,
			rows : numRows,
		}
	})(tokens)
	const buffers = {
		charFgColors : new Buffer(gl, "a_charFgColor", 3, MAX_CHARS * 6),
		charBgColor  : new Buffer(gl, "a_charBgColor", 3, MAX_CHARS * 6),
		fontOffsets  : new Buffer(gl, "a_fontOffset", 1, MAX_CHARS * 6),
		positions    : new Buffer(gl, "a_position", 2, MAX_CHARS * 6),
		uvs          : new Buffer(gl, "a_uv", 2, MAX_CHARS * 6),
	}
	const fontBitmaps = new Map()
	const glyphTex = gl.createTexture()
	{
		gl.bindTexture(gl.TEXTURE_2D, glyphTex)
		gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1)
		const level = 0
		const internalFormat = gl.LUMINANCE
		const texWidth = fontData.cols * fontData.fontW
		const texHeight = fontData.rows * fontData.fontH
		const texBorder = 0
		const srcFormat = gl.LUMINANCE
		const srcType = gl.UNSIGNED_BYTE
		const bits = getBitArrayFromData(fontData.data.join(""), texWidth * texHeight)
		const arr = bits.map(e => e * 255)
		gl.texImage2D(gl.TEXTURE_2D, level, internalFormat, texWidth, texHeight, texBorder, srcFormat, srcType, arr)
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE)
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE)
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST)
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST)
		for (let c = 0; c < fontData.charSet.length; c++) {
			const ch = fontData.charSet[c]
			const gcol = c % fontData.cols
			const grow = Math.floor(c / fontData.cols)
			const ox = gcol * fontData.fontW
			const oy = grow * fontData.fontH
			const grid = new Grid2(fontData.fontW, fontData.fontH, Uint8Array)
			for (let y = 0; y < fontData.fontH; y++) {
				for (let x = 0; x < fontData.fontW; x++) {
					grid.set(x, y, bits[(oy + y) * texWidth + (ox + x)])
				}
			}
			fontBitmaps.set(ch, grid)
		}
	}
	const palettes = PALETTES.map(e => {
		const tokens = {}
		if (e.tokens) {
			for (const sym of Object.getOwnPropertySymbols(e.tokens)) {
				tokens[sym] = int2rgb(e.tokens[sym])
			}
		}
		return {
			name : e.name,
			bg : int2rgb(e.bg),
			tokens,
		}
	})
	const DIRECTION_LOOKUP_MAP = [
		[ 0,  0],
		[ 0, -1],
		[ 1, -1],
		[ 1,  0],
		[ 1,  1],
		[ 0,  1],
		[-1,  1],
		[-1,  0],
		[-1, -1],
	]
	const chars = [
		new Grid2(0, 0, Array),
		new Grid2(0, 0, Array)
	]
	function clearCharBuffer(buf, pal) {
		buf.loop((char, x, y, idx) => {
			char.idx = SPACE
			char.bgColor = pal.bg
			char.fgColor = pal.tokens[TokenTypes.DEFAULT]
		})
	}
	const arrowKeys = {
		"ArrowUp"    : false,
		"ArrowDown"	 : false,
		"ArrowLeft"  : false,
		"ArrowRight" : false,
	}
	addEventListener("keyup", e => {
		if (e.key in arrowKeys) arrowKeys[e.key] = false
	})
	addEventListener("keydown", e => {
		if (e.key in arrowKeys) {
			e.preventDefault()
			scrollTimeout = SCROLL_TIMEOUT_DURATION
			arrowKeys[e.key] = true
			return
		}
		if (/^F\d/.test(e.key)) {
			e.preventDefault()
		}
		if (e.key == "Backspace") {
			promptStr = promptStr.slice(0, -1)
		} else if (e.key == "Enter") {
			promptStr += "\n"
		} else if (e.key == "F1") {
			showInfo = !showInfo
		} else if (e.key == "F2") {
			exportSize = 2
		} else if (e.key == "F3") {
			if (document.fullscreenElement) {
				document.exitFullscreen().catch(e => console.warn(e))
			} else if (document.body.requestFullscreen) {
				document.body.requestFullscreen().catch(e => console.warn(e))
			}
		} else if (e.key == "F4") {
			showLineNumbers = !showLineNumbers
		} else if (e.key == "F5") {
			desiredScale = Math.min(4, desiredScale + 1)
			forceResize = true
		} else if (e.key == "F6") {
			desiredScale = Math.max(1, desiredScale - 1)
			forceResize = true
		} else if (e.key == "F7") {
			rainbowMode = (rainbowMode + 1) % 3
		} else {
			if (fontData.charSet.includes(e.key)) {
				promptStr += e.key
				messageTimeout = MESSAGE_TIMEOUT_DURATION
			}
		}
	})
	canvas.addEventListener("pointerdown", e => {
		dragActive = true
		dragStartX = e.clientX
		dragStartY = e.clientY
		dragStartScrollX = scrollOffsetX
		dragStartScrollY = scrollOffsetY
		canvas.setPointerCapture(e.pointerId)
		scrollTimeout = SCROLL_TIMEOUT_DURATION
	})
	canvas.addEventListener("pointermove", e => {
		if (!dragActive) return
		const dx = e.clientX - dragStartX
		const dy = e.clientY - dragStartY
		const ix = Math.round(dx / ((fontData.fontW + settings.charSpacingX) * desiredScale))
		const iy = Math.round(-dy / (fontData.fontH + settings.charSpacingY))
		scrollOffsetX = ((dragStartScrollX + ix) % cols + cols) % cols
		scrollOffsetY = ((dragStartScrollY + iy) % lineSampler.rows + lineSampler.rows) % lineSampler.rows
		scrollTimeout = SCROLL_TIMEOUT_DURATION
	})
	const endDrag = e => {
		if (!dragActive) return
		dragActive = false
		if (e && e.pointerId != null && canvas.hasPointerCapture(e.pointerId)) {
			canvas.releasePointerCapture(e.pointerId)
		}
	}
	addEventListener("pointerup", endDrag)
	addEventListener("pointercancel", endDrag)
	addEventListener("resize", () => forceResize = true)
	addEventListener("message", e => {
		const d = e.data
		if (!d || typeof d !== "object") return
		if (d.type === "write") {
			promptStr = d.text
			messageTimeout = MESSAGE_TIMEOUT_DURATION
		} else if (d.type === "clear") {
			promptStr = ""
			clearCharBuffer(chars[0], pal)
			clearCharBuffer(chars[1], pal)
			shuffleHighlightMode()
			shuffleScrollMode()
			shuffleWarpMode()
			shuffleOffsets()
			maskHeight = 0
			dmaskHeight = 1
		} else if (d.type === "palette") {
			const p = parseInt(d.paletteId)
			if (p < 0 || p >= palettes.length) return
			pal = palettes[p]
		}
	})
	let forceResize = true
	let showInfo = false
	let showLineNumbers = settings.lineNumbers
	let exportSize = 0
	let desiredScale = Math.max(1, settings.scale)
	let pal
	if (settings.bgColor !== -1 && settings.defaultColor !== -1) {
		pal = {
			bg: int2rgb(settings.bgColor),
			tokens: { [TokenTypes.DEFAULT]: int2rgb(settings.defaultColor) }
		}
	} else {
		let p = settings.tokenId
		if (p < 0 || p >= palettes.length || Number.isNaN(p)) p = RND.minmaxInt(0, palettes.length)
		pal = palettes[p]
	}
	let cols = 0
	let rows = 0
	let warpFn = null
	let scrollDirX = 0
	let maskDirY = 0
	let maskOffsetY = 0
	let maskHeight = 0
	let dmaskHeight = 0
	let scrollOffsetX = 0
	let scrollOffsetY = 0
	let scrollDirY = 0
	let dragActive = false
	let dragStartX = 0
	let dragStartY = 0
	let dragStartScrollX = 0
	let dragStartScrollY = 0
	const SCROLL_TIMEOUT_DURATION = 30
	let scrollTimeout = 0
	const MESSAGE_TIMEOUT_DURATION = 30
	let messageTimeout = 0
	let promptStr = ""
	let rainbowMode = 0
	let lineModulo = 0
	let highlightTokens = []
	function shuffleOffsets() {
		scrollOffsetX = 4
		scrollOffsetY = RND.minmaxInt(0, lineSampler.rows)
		maskOffsetY = 0
	}
	function shuffleHighlightMode() {
		if (RND.probability(0.5)) {
			const n = RND.minmaxInt(0, 4)
			const t = [
				TokenTypes.KEYWORD,
				TokenTypes.IDENTIFIER,
				TokenTypes.NUMBER,
				TokenTypes.STRING,
				TokenTypes.COMMENT,
				TokenTypes.OPERATOR,
				TokenTypes.PUNCTUATION,
			]
			highlightTokens = RND.shuffleArray(t).slice(0, n)
			lineModulo = 0
		} else {
			lineModulo = RND.chooseWeighted([
				[3, 0],
				[1, 2],
				[1, 3],
				[1, 4],
			])
			highlightTokens = []
		}
	}
	function shuffleScrollMode() {
		const r = RND.minmaxInt(0, 4)
		switch (r) {
			case 0:
				scrollDirX = RND.choose(-1, 0, 1)
				break
			case 1:
				maskDirY = RND.choose(-1, 1)
				break
			case 2:
				scrollDirY = RND.choose(-1, 0, 1)
				break
			case 3:
				dmaskHeight = RND.choose(rows, 1, RND.minmaxInt(1, rows))
				break
		}
	}
	function shuffleWarpMode() {
		const w = RND.choose(Object.keys(WarpFunctions))
		warpFn = WarpFunctions[w](RND, NOISE)
	}
	async function render(frame, fps) {
		if (forceResize) {
			forceResize = false
			const cw = settings.canvasWidth || innerWidth
			const ch = settings.canvasHeight || innerHeight
			canvas.width = cw
			canvas.height = ch
			const scale = desiredScale
			const marginX = 0
			const marginY = 0
			const spacingX = settings.charSpacingX
			const spacingY = settings.charSpacingY
			cols = Math.floor((cw - marginX * 2) / ((fontData.fontW + spacingX) * scale))
			rows = Math.floor((ch - marginY * 2) / ((fontData.fontH + spacingY) * scale))
			if (cols * rows >= MAX_CHARS) {
				rows = Math.floor(MAX_CHARS / cols)
			}
			cols = Math.max(2, cols)
			rows = Math.max(2, rows)
			const ox = Math.floor((cw - (cols * fontData.fontW + (cols - 1) * spacingX) * scale) / 2)
			const oy = Math.floor((ch - (rows * fontData.fontH + (rows - 1) * spacingY) * scale) / 2)
			const rw = fontData.fontW * scale
			const rh = fontData.fontH * scale
			const gx = (fontData.fontW + spacingX) * scale
			const gy = (fontData.fontH + spacingY) * scale
			let idx = 0
			for (let j = 0; j < rows; j++) {
				for (let i = 0; i < cols; i++) {
					const x = ox + i * gx
					const y = oy + j * gy
					emitTexturedBox(idx++, buffers.positions.data, buffers.uvs.data, x, y, rw, rh)
				}
			}
			buffers.uvs.enable(textProgram.program)
			buffers.uvs.bufferData(gl.STATIC_DRAW)
			buffers.uvs.needsUpdate = true
			buffers.positions.enable(textProgram.program)
			buffers.positions.bufferData(gl.STATIC_DRAW)
			buffers.positions.needsUpdate = true
			chars[0].resize(cols, rows).fill( (x,y) => new Char(x,y))
			chars[1].resize(cols, rows).fill( (x,y) => new Char(x,y))
			clearCharBuffer(chars[0], pal)
			clearCharBuffer(chars[1], pal)
			dmaskHeight = rows
			maskHeight = 0
			shuffleHighlightMode()
			shuffleScrollMode()
			shuffleWarpMode()
			shuffleOffsets()
		}
		const meltTick = (frame % settings.frameDivider == 0)
		const meltFrame = frame / settings.frameDivider
		if (meltTick) {
			if (RND.probability(0.01)) {
				shuffleWarpMode()
			}
			if (RND.probability(0.02)) {
				shuffleScrollMode()
			}
			if (RND.probability(0.02) && maskHeight == 1) {
				shuffleHighlightMode()
			}
			if (scrollTimeout == 0) {
				maskOffsetY = ((maskOffsetY + maskDirY) % rows + rows) % rows
				scrollOffsetX  = ((scrollOffsetX  + scrollDirX) % cols + cols) % cols
				scrollOffsetY = ((scrollOffsetY + scrollDirY) % lineSampler.rows + lineSampler.rows) % lineSampler.rows
			}
			if (messageTimeout == 0) {
				promptStr = promptStr.replace(/\S/, " ").trimEnd()
			}
			scrollTimeout = Math.max(0, scrollTimeout - 1)
			messageTimeout = Math.max(0, messageTimeout - 1)
			if (maskHeight < dmaskHeight) maskHeight = Math.min(maskHeight + 1, rows)
			else if (maskHeight > dmaskHeight) maskHeight = Math.max(maskHeight - 1, 1)
		}
		const arrowHeld = arrowKeys.ArrowUp || arrowKeys.ArrowDown || arrowKeys.ArrowLeft || arrowKeys.ArrowRight
		if (arrowHeld || dragActive) {
			scrollTimeout = SCROLL_TIMEOUT_DURATION
		}
		if (scrollTimeout > 0) {
			dmaskHeight = rows
			if (arrowKeys.ArrowUp) {
				scrollOffsetY = (scrollOffsetY + 1) % lineSampler.rows
			} else if (arrowKeys.ArrowDown) {
				scrollOffsetY = (scrollOffsetY - 1 + lineSampler.rows) % lineSampler.rows
			}
			if (arrowKeys.ArrowLeft) {
				scrollOffsetX = (scrollOffsetX - 1 + cols) % cols
			} else if (arrowKeys.ArrowRight) {
				scrollOffsetX = (scrollOffsetX + 1) % cols
			}
		}
		const fontAspect = fontData.fontW / fontData.fontH
		const gridAspect = cols / rows
		const aspect = gridAspect * fontAspect
		if (meltTick) {
			const src = chars[0]
			const dst = chars[1]
			for (let j=0; j<rows; j++) {
				for (let i=0; i<cols; i++) {
					const s = (i / cols * 2 - 1) * aspect
					const t = (j / rows * 2 - 1)
					const w = warpFn.fn(s, t, i, j, meltFrame)
					const readFrom = DIRECTION_LOOKUP_MAP[w].map( (e, idx) => e + [i, j][idx])
					const a = src.getWrapped(...readFrom)
					const b = dst.get(i, j)
					if (a != null) {
						b.cloneFrom(a)
					}
				}
			}
			chars.reverse(); // flip-flop
		}
		const curr = chars[0];
		// mask
		for (let j=0; j < maskHeight; j++) {
			const row = (maskOffsetY + j) % rows
			const lineNum = (scrollOffsetY + row) % lineSampler.rows
			const line = lineSampler.getLine(lineNum)
			const lineHighlight = lineModulo == 0 ? false : lineNum % lineModulo == 0
			for (let i=0; i<line.length; i++) {
				const token = line[i]
				if (token.char == "\t") continue
				const col = (i + scrollOffsetX) % cols
				const char = curr.get(col, row)
				char.idx = CHAR_LOOKUP[token?.char] ?? SPACE
				const fg = pal.tokens[token?.type] ?? pal.tokens[TokenTypes.DEFAULT]
				const bg = pal.bg
				const inverted = lineHighlight || highlightTokens.includes(token.type)
				char.fgColor = inverted ? bg : fg
				char.bgColor = inverted ? fg : bg
			}
		}
		if (showLineNumbers) {
			const maxNumWidth = String(lineSampler.rows).length
			for (let j = 0; j < maskHeight; j++) {
				const row = (maskOffsetY + j) % rows
				const lineNum = (scrollOffsetY + row) % lineSampler.rows
				const lineStr = String(lineNum).padStart(maxNumWidth, " ") + " "
				for (let i = 0; i < lineStr.length; i++) {
					const char = curr.get(i, row)
					char.idx = CHAR_LOOKUP[lineStr[i]]
					const fg = pal.tokens[TokenTypes.NUMBER] ?? pal.tokens[TokenTypes.DEFAULT]
					const bg = pal.bg
					if (lineNum % 2 == 0) {
						char.fgColor = fg
						char.bgColor = bg
					} else {
						char.fgColor = bg
						char.bgColor = fg
					}
				}
			}
		}
		// wave
		{
			function envelope(t) {
				if (t <= 0 || t >= 2) return 0
				const x = t < 1 ? t : 2 - t
				return x * x * (3 - 2 * x)
			}
			for (let j=0; j<rows; j++) {
				for (let i=0; i<cols; i++) {
					const char = curr.get(i, j)
					const u = i / cols * aspect
					const v = j / rows
					const t = (v * 2 + meltFrame * 0.01 + (settings.waveLength / 2)) % settings.waveLength
					const wave = envelope(t)
					const n = NOISE(u, v, meltFrame * 0.002)
					const idx = char.idx / fontData.charSet.length
					const s = wave * n + idx * 0.3 + 0.2
					if (s > 0.5) {
						char.bgColor = pal.bg
						char.idx = SPACE
					}
				}
			}
		}
		// overlay
		{
			const FIT_COLS = 4
			const FIT_ROWS = 2
			const scale = Math.max(1, Math.floor(Math.min(
				cols / (FIT_COLS * fontData.fontW),
				rows / (FIT_ROWS * (fontData.fontH - 4))
			)))
			const lineH = (fontData.fontH - 3) * scale
			const startX = 0
			let offsX = startX
			let offsY = -2 * scale
			for (const v of promptStr) {
				if (v === '\n') {
					offsX = startX
					offsY += lineH
					continue
				}
				if (!fontBitmaps.has(v)) continue
				const bigChar = fontBitmaps.get(v)
				const charW = bigChar.width * scale
				if (offsX + charW > cols) {
					offsX = startX
					offsY += lineH
				}
				const charIndex = CHAR_LOOKUP[v]
				let col
				if (rainbowMode == 0) {
					col = pal.tokens[TokenTypes.DEFAULT]
				} else if (rainbowMode == 1) {
					col = [
						RND.float(),
						RND.float(),
						RND.float(),
					]
				}
				bigChar.loop( (val, x, y) => {
					if (val == 1) {
						for (let dy = 0; dy < scale; dy++) {
							for (let dx = 0; dx < scale; dx++) {
								const cx = x * scale + dx + offsX
								const cy = y * scale + dy + offsY
								const char = curr.get(cx, cy)
								if (char == null) continue
								const c = fontData.charSet[Math.floor((meltFrame + charIndex) % fontData.charSet.length)]
								char.idx = CHAR_LOOKUP[c]
								if (rainbowMode == 2) {
									const coffs = cy / rows * Math.PI * 2
									col = [
										Math.sin(frame * 0.11 + coffs) * 0.5 + 0.5,
										Math.sin(frame * 0.12 + coffs) * 0.5 + 0.5,
										Math.sin(frame * 0.13 + coffs) * 0.5 + 0.5,
									]
								}
								char.fgColor = col
								char.bgColor = pal.bg
							}
						}
					}
				})
				offsX += charW
			}
		}
		if (showInfo) {
			const HR = "~F~"
			const COL = 21
			let out = ""
			out += HR + "\n"
			out += "fps (vsync)".padEnd(COL) + fps + "\n"
			out += "frame".padEnd(COL) + shortenString(frame, COL - 1) + "\n"
			out += HR + "\n"
			for (const key in settings) {
				out += key.padEnd(COL) + shortenString(settings[key], COL - 1) + "\n"
			}
			out += HR + "\n"
			out += "cols".padEnd(COL) + cols + "\n"
			out += "rows".padEnd(COL) + rows + "\n"
			out += "innerWidth".padEnd(COL) + innerWidth + "\n"
			out += "innerHeight".padEnd(COL) + innerHeight + "\n"
			out += HR + "\n"
			out += "keys".padEnd(COL - 3) + "F1 toggle info\n"
			out += "F2".padEnd(3).padStart(COL) + "save PNG\n"
			out += "F3".padEnd(3).padStart(COL) + "fullscreen\n"
			out += "F4".padEnd(3).padStart(COL) + "toggle line numbers\n"
			out += "F5/F6".padEnd(6).padStart(COL) + "pixel scale\n"
			out += "currsors".padEnd(9).padStart(COL) + "scroll\n"
			out += HR + "\n"
			const maxLen = COL * 2
			out.trim().replaceAll(HR, "".padEnd(maxLen, "-")).split("\n").forEach((str, line_idx) => {
				str.padEnd(maxLen, " ").split("").forEach((chr, char_idx) => {
					const char = curr.get(char_idx, line_idx)
					if (char !== null) {
						char.idx = CHAR_LOOKUP[chr]
						char.fgColor = pal.tokens[TokenTypes.DEFAULT]
						char.bgColor = pal.bg
					}
				})
			})
		}
		for (let j=0; j<rows; j++) {
			for (let i=0; i<cols; i++) {
				const idx = j * cols + i
				const char = curr.get(i, j)
				emitOffset(idx, buffers.fontOffsets.data, char.idx)
				emitColors(idx, buffers.charFgColors.data, char.fgColor )
				emitColors(idx, buffers.charBgColor.data, char.bgColor )
			}
		}
		buffers.fontOffsets.needsUpdate = true
		buffers.charFgColors.needsUpdate = true
		buffers.charBgColor.needsUpdate = true
		gl.bindTexture(gl.TEXTURE_2D, null)
		gl.bindFramebuffer(gl.FRAMEBUFFER, null)
		gl.viewport(0, 0, gl.canvas.width, gl.canvas.height)
		const textureUnit = 0
		gl.activeTexture(gl.TEXTURE0 + textureUnit)
		gl.bindTexture(gl.TEXTURE_2D, glyphTex)
		textProgram.use()
		textProgram.set("u_resolution", gl.canvas.width, gl.canvas.height)
		textProgram.set("u_fontTableSize", fontData.cols, fontData.rows)
		textProgram.set("u_texture", textureUnit)
		Object.keys(buffers).forEach(e => {
			if (buffers[e].needsUpdate) {
				buffers[e].enable(textProgram.program)
				buffers[e].bufferData(gl.DYNAMIC_DRAW)
				buffers[e].needsUpdate = false
			}
		})
		gl.clearColor(...(pal.bg), 1)
		gl.clear(gl.COLOR_BUFFER_BIT)
		const NUM_QUADS_TO_DRAW = cols * rows
		gl.drawArrays(gl.TRIANGLES, 0, NUM_QUADS_TO_DRAW * 6)
		if (exportSize > 0) {
			const outW = canvas.width * exportSize
			const outH = canvas.height * exportSize
			const c = getExportCanvas()
			c.width = outW
			c.height = outH
			const ctx = c.getContext("2d")
			ctx.imageSmoothingEnabled = false
			ctx.drawImage(canvas, 0, 0, outW, outH)
			const name = PROJECT_NAME + "_" + pal.name + "_" + Date.now() + ".png"
			savePNG(name, c)
			exportSize = 0
		}
	}
	return {
		render: render,
		canvas: canvas
	}
}
function int2rgb(int) {
	return [
		(int >> 8 & 0xf) / 15.0,
		(int >> 4 & 0xf) / 15.0,
		(int      & 0xf) / 15.0
	]
}
function getUniqueChars(source) {
	const counts = new Map()
	for (const ch of source) counts.set(ch, (counts.get(ch) || 0) + 1)
	return [...counts.keys()]
}
function shortenString(str, max) {
	const s = String(str)
	if (s.length > max) {
		return "..." + s.substring(s.length - max + 3)
	}
	return s
}
const getExportCanvas = (function() {
	let exportCanvas = null
	return function() {
		if (!exportCanvas) {
			exportCanvas = new OffscreenCanvas(1, 1)
		}
		return exportCanvas
	}
}())
const saveBlob = (function() {
	const a = document.createElement('a')
	document.body.appendChild(a)
	a.style.display = 'none'
	return function(filename, blob) {
		const url = URL.createObjectURL(blob)
		a.href = url
		a.download = filename
		a.click()
		URL.revokeObjectURL(url)
	}
}())
function savePNG(filename, canvas) {
	if (canvas.convertToBlob) {
		canvas.convertToBlob({ type: "image/png" }).then(blob => saveBlob(filename, blob))
	} else {
		canvas.toBlob(blob => saveBlob(filename, blob))
	}
}
class FPS {
	constructor(updateRateMS = 1000) {
		this.updateRateMS = updateRateMS
		this.fps = 0
		this.frames = 0
		this.ptime = 0
	}
   	tick(currentTime){
		if ( currentTime >= this.ptime + this.updateRateMS ) {
			this.fps = this.frames * this.updateRateMS  / ( currentTime - this.ptime )
			this.ptime = currentTime
			this.frames = 0
		}
		this.frames++
		return this.fps
	}
}
const TYPES = {
	STRING: Symbol(),
	INT: Symbol(),
	FLOAT: Symbol(),
	COLOR_HEX: Symbol(),
}
function sanitize(value, expectedType) {
	if (value === null || value === undefined) return null
	let convertedValue = null
	if (expectedType === TYPES.STRING) {
		convertedValue = String(value)
	}
	else if (expectedType === TYPES.INT) {
		const parsed = parseFloat(value)
		if (!isNaN(parsed)) {
			convertedValue = Math.floor(parsed)
		}
	}
	else if (expectedType === TYPES.FLOAT) {
		const parsed = parseFloat(value)
		if (!isNaN(parsed)) {
			convertedValue = parsed
		}
	}
	else if (expectedType === TYPES.COLOR_HEX) {
		let hex = String(value).replace('#', '').trim()
		if (hex.length === 6) hex = hex[0] + hex[2] + hex[4]
		if (/^[0-9A-Fa-f]{3}$/.test(hex)) {
			convertedValue = parseInt(hex, 16)
		}
	}
	return convertedValue
}
function mergeSettings(defaultSettings, userSettings) {
	const mergedSettings = {}
	Object.keys(defaultSettings).forEach(key => {
		const defaultSetting = defaultSettings[key]
		const validatedValue = sanitize(defaultSetting.value, defaultSetting.type)
		mergedSettings[key] = validatedValue !== null ? validatedValue : defaultSetting.value
	})
	for (const [key, userValue] of userSettings) {
		const defaultSetting = defaultSettings[key]
		if (!defaultSetting) continue
		const convertedValue = sanitize(userValue, defaultSetting.type)
		if (convertedValue !== null) {
			mergedSettings[key] = convertedValue
		}
	}
	return mergedSettings
}
const DEFAULT_SETTINGS = Object.freeze({
	"fps"          : { type: TYPES.INT,       value: 60 },
	"frameDivider" : { type: TYPES.INT,       value: 3 },
	"scale"        : { type: TYPES.INT,       value: 1 },
	"canvasWidth"  : { type: TYPES.INT,       value: 0 },
	"canvasHeight" : { type: TYPES.FLOAT,     value: 0 },
	"seed"         : { type: TYPES.STRING,    value: Math.random().toString(16).substring(2) },
	"charSpacingX" : { type: TYPES.INT,       value: 0 },
	"charSpacingY" : { type: TYPES.INT,       value: 0 },
	"maxChars"     : { type: TYPES.INT,       value: 256 * 256 },
	"tokenId"      : { type: TYPES.INT,       value: -1 },
	"padSpace"     : { type: TYPES.BOOLEAN,   value: true },
	"lineNumbers"  : { type: TYPES.BOOLEAN,   value: false },
	"waveLength"   : { type: TYPES.INT,       value: 8 },
	"bgColor"      : { type: TYPES.COLOR_HEX, value: -1 },
	"defaultColor" : { type: TYPES.COLOR_HEX, value: -1 },
})
const USER_SETTINGS = new URLSearchParams(window.location.search)
const settings = mergeSettings(DEFAULT_SETTINGS, USER_SETTINGS)
settings.tokenId = settings.tokenId !== -1
	? settings.tokenId
	: globalThis.tokenData?.tokenId ?? -1
Object.freeze(settings)
const app = await boot(document.querySelector("canvas"), settings)
const fps = new FPS(1000)
let ptime = -Infinity
let frame = 0
requestAnimationFrame(loop)
function loop(time) {
	requestAnimationFrame(loop)
	const currentFPS = Math.round(fps.tick(time))
	const frameMS = 1000 / settings.fps
	if (time - ptime + 1 >= frameMS) {
		ptime = time
		app.render(frame, currentFPS)
		frame++
	}
}

	