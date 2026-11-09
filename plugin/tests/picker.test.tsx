import { expect, mock, test } from 'claude-code/testing'
import type { On, ProcessRunInit } from 'claude-code'

import { parse, parseStock, stockPayload } from '../hooks/register'

const SKILL = 'dccstatusline:theme'
const BIN = '/home/u/.local/bin/dccstatusline'
const CONFIG = '/home/u/.config/dccstatusline/config'

// The profile has no default: a config holding none opens the row at basic,
// which every --themes run then lays.
const BASIC = '[statusline]\nprofile = basic\n'

// Two lines of `dccstatusline --themes` over an overlay, one a preset, cut
// short after the branch. A separator key lays ┊ on them, whatever it names.
// Nothing else in the overlay shows. No theme is no preset, so it has no line.
function themes(overlay: string) {
  const sep = overlay.includes('separator = ') ? ' ┊ ' : ' │ '

  return (
    `tokyonight  \x1b[0;38;2;122;162;247m~/src/acme\x1b[0;38;2;86;95;137m${sep}` +
    '\x1b[0;38;2;158;206;106mmain\x1b[0m\n' +
    `nord        \x1b[0;38;2;136;192;208m~/src/acme\x1b[0;38;2;97;110;136m${sep}` +
    '\x1b[0;38;2;136;192;208mmain\x1b[0m\n'
  )
}

// A plain run on no config, cut short after one figure: the line with no theme,
// unlabeled. No repository answers at its cwd, so it has no branch. It takes no
// overlay, so it keeps │ and its figure stays bright_cyan whatever the options.
const STOCK = '\x1b[0;94m~/src/acme\x1b[0;90m │ \x1b[0;96m42%\x1b[0m\n'

const PANE = {
  plugin: 'dccstatusline',
  component: 'Pane',
  requestId: 'theme-picker',
  surface: 'terminal',
  props: {
    title: 'dccstatusline themes',
    isFocused: true,
    bodyColumns: 120,
    placement: 'inline',
    scroll: { offset: 0, bodyRows: 10 },
    view: {},
  },
} as const

// `/dccstatusline:theme` as the person types it, on a 97-column terminal.
const TYPED = {
  command: SKILL,
  args: '',
  origin: { kind: 'composer' },
  presentation: { isFullscreen: false, columns: 97 },
} as const

const RAN = { exitCode: 0, stderr: '', isStdoutTruncated: false, isStderrTruncated: false }

// Nothing sits beneath the plugin: these hooks stand for the engine. A status
// line that runs `command`, a config that by default names tokyonight alone,
// and `--themes` and the plain run, whose argv land in the array returned and
// whose options land in inits.
function engine(
  on: On,
  {
    command = '~/.local/bin/dccstatusline',
    config = '[statusline]\ntheme = tokyonight\n',
    inits = [] as (ProcessRunInit | undefined)[],
  } = {},
) {
  const argv: string[][] = []

  mock.env(on, { HOME: '/home/u' })
  on('settings.read', () => ({ value: { statusLine: { type: 'command', command } } }))
  on('fs.read', (_, e) => {
    if (e.path !== CONFIG) throw new Error(`${e.path}: no such file`)

    return { value: config }
  })
  on('process.run', (_, e) => {
    argv.push([...e.argv])
    inits.push(e.init)

    return { value: { ...RAN, stdout: e.argv.length === 1 ? STOCK : themes(e.argv[2] ?? '') } }
  })

  return argv
}

// The skill beneath the plugin's hook: each run lands in the array returned.
function skill(on: On) {
  const runs: string[] = []

  on('command.run', (_, e) => {
    runs.push(`/${e.command} ${e.args}`.trim())

    return { text: '' }
  })

  return runs
}

test('the theme row starts at the one in use, Enter moves it round, and apply names it', async ($, on) => {
  const argv = engine(on)
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))

  const ran = await $.command.run(TYPED)

  expect(ran.text).toMatch(/Pick a theme/)
  expect(argv).toEqual([[BIN, '--themes', BASIC], [BIN]])
  expect(runs).toEqual([])

  const ui = await $.ui.mount(PANE)
  const branches = async () => (await ui.findAll({ type: 'Text', text: /^main$/ })).map(t => t.props.color)

  // One sample, in the chosen theme's colors.
  expect(await branches()).toEqual(['#9ece6a'])
  expect(await ui.find({ type: 'Text', text: /^\s+tokyonight$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /in use: tokyonight/ })).toBeDefined()

  // On, then round to the first, from what --themes already listed. Nothing
  // is applied.
  await ui.press({ key: 'theme' })
  expect(await branches()).toEqual(['#88c0d0'])
  await ui.press({ key: 'theme' })
  expect(await branches()).toEqual([])
  expect(await ui.find({ type: 'Text', text: /^\s+no theme$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /in use: tokyonight/ })).toBeDefined()
  expect(argv).toHaveLength(2)
  expect(runs).toEqual([])

  await ui.press({ key: 'theme' })
  expect(await branches()).toEqual(['#9ece6a'])
  await ui.press({ key: 'theme' })
  await ui.press({ key: 'theme' })

  // No theme goes to the skill as none.
  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} none`])
  await ui.unmount()
})

test('the preview sits in a box above the rows', async ($, on) => {
  engine(on)
  skill(on)

  let rows: number | undefined

  on('ui.open', (_, e) => {
    rows = e.rows

    return { value: { isPlaced: true } }
  })
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)

  // Six rows of buttons, the box's two borders, the blank line under the box,
  // the rows and Apply, and ten for the preview and the hint to wrap into at 30
  // columns.
  expect(rows).toBe(21)

  const ui = await $.ui.mount(PANE)
  const buttons = await ui.findAll({ type: 'Button' })

  expect(buttons.map(b => b.key)).toEqual(['theme', 'palette', 'sep', 'profile', 'glyphs', 'apply'])
  expect(buttons[0]?.props.autoFocus).toBe(true)

  // The properties read in title case, and Apply is the one marked as the main
  // action, which a plain Button can't be.
  for (const label of ['Theme', 'Palette', 'Separator', 'Profile', 'Glyphs', 'Apply']) {
    expect(await ui.find({ type: 'Button', text: label })).toBeDefined()
  }

  expect(buttons.at(-1)?.props.variant).toBe('primary')
  expect(buttons.at(-1)?.props.plain).toBeUndefined()

  const root = await ui.drawn()
  const children = root.type === 'Box' ? root.children : undefined
  const first = children?.at(0)

  // The box, the rows, Apply and the hint, a blank line between each.
  expect(root).toMatchObject({ type: 'Box', props: { gap: 1 } })
  expect(children).toHaveLength(4)
  expect(first).toMatchObject({ type: 'Box', props: { borderStyle: 'round' } })
  expect(JSON.stringify(first)).toContain('~/src/acme')
  await ui.unmount()
})

test('a config naming a theme the binary lacks starts at no theme', async ($, on) => {
  engine(on, { config: '[statusline]\ntheme = bogus\n' })
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)

  const ui = await $.ui.mount(PANE)

  expect(await ui.find({ type: 'Text', text: /^\s+no theme$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /in use: bogus/ })).toBeDefined()

  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} none`])
  await ui.unmount()
})

test('an option reruns --themes with its INI laid over, and the pick names it', async ($, on) => {
  const argv = engine(on)
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)

  const ui = await $.ui.mount(PANE)

  expect(await ui.find({ type: 'Text', text: /\s│$/ })).toBeDefined()

  // The theme row holds through the redraw.
  await ui.press({ key: 'theme' })
  await ui.press({ key: 'sep' })
  expect(argv.at(-1)).toEqual([BIN, '--themes', '[statusline]\nseparator = " ❯ "\n' + BASIC])
  expect(await ui.find({ type: 'Text', text: /\s❯$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /^\s+nord$/ })).toBeDefined()

  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} nord sep=❯ profile=basic`])
  await ui.unmount()
})

test('the profile row opens at basic, moves to detailed and round to basic, and the pick names it', async ($, on) => {
  const argv = engine(on)
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)
  expect(argv).toEqual([[BIN, '--themes', BASIC], [BIN]])

  const ui = await $.ui.mount(PANE)

  expect(await ui.find({ type: 'Text', text: /\sbasic$/ })).toBeDefined()

  await ui.press({ key: 'profile' })
  expect(argv.at(-1)).toEqual([BIN, '--themes', '[statusline]\nprofile = detailed\n'])
  expect(await ui.find({ type: 'Text', text: /\sdetailed$/ })).toBeDefined()

  // detailed is the last value, and there is no default past it.
  await ui.press({ key: 'profile' })
  expect(argv.at(-1)).toEqual([BIN, '--themes', BASIC])
  expect(await ui.find({ type: 'Text', text: /\sbasic$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /\snone$/ })).toBeUndefined()

  await ui.press({ key: 'profile' })
  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} tokyonight profile=detailed`])
  await ui.unmount()
})

test('the palette row opens at tritone, then previews duotone, then monochrome', async ($, on) => {
  const argv = engine(on)
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)

  const ui = await $.ui.mount(PANE)
  const palette = await ui.find({ type: 'Text', text: /\stritone$/ })

  // The row is live on every preset.
  expect(palette?.props.dimColor).toBeUndefined()

  await ui.press({ key: 'palette' })
  expect(argv.at(-1)).toEqual([BIN, '--themes', '[statusline]\npalette = duotone\n' + BASIC])
  expect(await ui.find({ type: 'Text', text: /\sduotone$/ })).toBeDefined()

  // Its INI comes first, so the options below it still land on top.
  await ui.press({ key: 'sep' })
  expect(argv.at(-1)).toEqual([
    BIN,
    '--themes',
    '[statusline]\npalette = duotone\n[statusline]\nseparator = " ❯ "\n' + BASIC,
  ])

  await ui.press({ key: 'palette' })
  expect(argv.at(-1)).toEqual([
    BIN,
    '--themes',
    '[statusline]\npalette = monochrome\n[statusline]\nseparator = " ❯ "\n' + BASIC,
  ])
  expect(await ui.find({ type: 'Text', text: /\smonochrome$/ })).toBeDefined()

  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} tokyonight palette=monochrome sep=❯ profile=basic`])
  await ui.unmount()
})

test('a config holding a palette opens at it, and moved back names tritone', async ($, on) => {
  const argv = engine(on, { config: '[statusline]\ntheme = tokyonight\npalette = monochrome\n' })
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)
  // One run draws every row.
  expect(argv).toEqual([[BIN, '--themes', '[statusline]\npalette = monochrome\n' + BASIC], [BIN]])

  const ui = await $.ui.mount(PANE)

  expect(await ui.find({ type: 'Text', text: /\smonochrome$/ })).toBeDefined()

  // monochrome is the last value, so the next is the first.
  await ui.press({ key: 'palette' })
  expect(argv.at(-1)).toEqual([BIN, '--themes', BASIC])
  expect(await ui.find({ type: 'Text', text: /\stritone$/ })).toBeDefined()

  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} tokyonight palette=tritone profile=basic`])
  await ui.unmount()
})

test('on no theme every option waits for a theme, and keeps its choice', async ($, on) => {
  const argv = engine(on)
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)

  const ui = await $.ui.mount(PANE)
  const dimmed = async () => (await ui.findAll({ type: 'Button' })).filter(b => b.props.dimColor).map(b => b.key)
  const text = async () => JSON.stringify(await ui.drawn())
  const figure = async () => (await ui.find({ type: 'Text', text: /^42%$/ }))?.props.color

  // One run per change draws every preset; the stock line was drawn at open.
  for (const key of ['palette', 'sep', 'profile', 'glyphs']) await ui.press({ key })
  expect(argv).toHaveLength(6)
  expect(argv.at(-1)).toEqual([
    BIN,
    '--themes',
    '[statusline]\npalette = duotone\n[statusline]\nseparator = " ❯ "\n' +
      '[statusline]\nprofile = detailed\n[statusline]\nglyphs = on\n',
  ])

  // The theme row moves onto no theme without running the binary again. Its line
  // is the stock one, and every option row waits for a theme.
  await ui.press({ key: 'theme' })
  await ui.press({ key: 'theme' })
  expect(argv).toHaveLength(6)
  expect(await ui.find({ type: 'Text', text: /^\s+no theme$/ })).toBeDefined()
  expect(await text()).toContain(' │ ')
  expect(await text()).not.toContain(' ┊ ')
  expect(await figure()).toBe('cyanBright')
  expect(await dimmed()).toEqual(['palette', 'sep', 'profile', 'glyphs'])

  const waiting = await ui.findAll({ type: 'Text', text: /\spick a theme$/ })

  expect(waiting.map(t => t.props.dimColor)).toEqual([true, true, true, true])

  for (const value of ['duotone', '❯', 'detailed', 'on']) {
    expect(await ui.findAll({ type: 'Text', text: new RegExp(`\\s${value}$`) })).toHaveLength(0)
  }

  // Enter on any of them does nothing there.
  for (const key of ['palette', 'sep', 'profile', 'glyphs']) await ui.press({ key })
  expect(argv).toHaveLength(6)

  // On a preset every option comes back as it was.
  await ui.press({ key: 'theme' })
  expect(await dimmed()).toEqual([])

  for (const value of ['duotone', '❯', 'detailed', 'on']) {
    expect(await ui.find({ type: 'Text', text: new RegExp(`\\s${value}$`) })).toBeDefined()
  }

  await ui.press({ key: 'theme' })
  await ui.press({ key: 'theme' })

  // No theme goes as none, alone: every option waits.
  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} none`])
  await ui.unmount()
})

test('default, the v0.0.6 stock line, opens on no theme with every option waiting', async ($, on) => {
  const argv = engine(on, {
    config:
      '[statusline]\ntheme = default\npalette = duotone\nseparator = " ❯ "\nprofile = basic\nglyphs = on\n',
  })
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)
  expect(argv).toEqual([
    [
      BIN,
      '--themes',
      '[statusline]\npalette = duotone\n[statusline]\nseparator = " ❯ "\n' +
        '[statusline]\nprofile = basic\n[statusline]\nglyphs = on\n',
    ],
    [BIN],
  ])

  const ui = await $.ui.mount(PANE)

  expect(await ui.find({ type: 'Text', text: /^\s+no theme$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /in use: no theme/ })).toBeDefined()
  expect(JSON.stringify(await ui.drawn())).not.toContain(' ┊ ')
  expect((await ui.find({ type: 'Text', text: /^42%$/ }))?.props.color).toBe('cyanBright')
  expect((await ui.findAll({ type: 'Button' })).filter(b => b.props.dimColor).map(b => b.key)).toEqual([
    'palette',
    'sep',
    'profile',
    'glyphs',
  ])
  expect(await ui.findAll({ type: 'Text', text: /\spick a theme$/ })).toHaveLength(4)

  // On a preset the options the config holds come back.
  await ui.press({ key: 'theme' })

  for (const value of ['duotone', '❯', 'basic', 'on']) {
    expect(await ui.find({ type: 'Text', text: new RegExp(`\\s${value}$`) })).toBeDefined()
  }

  await ui.press({ key: 'theme' })
  await ui.press({ key: 'theme' })

  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} none`])
  await ui.unmount()
})

test('a config whose profile is none opens at basic, and its glyphs are live', async ($, on) => {
  // none is no value of the row's, so the row starts at its first, and the
  // glyphs key the config holds draws under it.
  const argv = engine(on, { config: '[statusline]\ntheme = tokyonight\nprofile = none\nglyphs = on\n' })
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)
  expect(argv).toEqual([[BIN, '--themes', BASIC + '[statusline]\nglyphs = on\n'], [BIN]])

  const ui = await $.ui.mount(PANE)

  expect(await ui.find({ type: 'Text', text: /\sbasic$/ })).toBeDefined()
  expect((await ui.find({ type: 'Text', text: /\son$/ }))?.props.dimColor).toBeUndefined()
  expect((await ui.findAll({ type: 'Button' })).filter(b => b.props.dimColor)).toHaveLength(0)

  await ui.press({ key: 'glyphs' })
  expect(argv.at(-1)).toEqual([BIN, '--themes', BASIC + '[statusline]\nglyphs = nerdfont\n'])
  expect(await ui.find({ type: 'Text', text: /\snerdfont$/ })).toBeDefined()

  // Round to the first value, which writes nothing. The profile stays.
  await ui.press({ key: 'glyphs' })
  expect(argv.at(-1)).toEqual([BIN, '--themes', BASIC])
  expect(await ui.find({ type: 'Text', text: /\soff$/ })).toBeDefined()

  await ui.press({ key: 'glyphs' })
  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} tokyonight profile=basic glyphs=on`])
  await ui.unmount()
})

test('the options start where the config is, and one moved back to its first value is named', async ($, on) => {
  const argv = engine(on, {
    config:
      '[statusline]\ntheme = tokyonight\nseparator = " ┋ "\nprofile = basic   # just the figures\n' +
      'glyphs = on\n',
  })
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)

  expect(argv).toEqual([
    [
      BIN,
      '--themes',
      '[statusline]\nseparator = " ┋ "\n[statusline]\nprofile = basic\n[statusline]\nglyphs = on\n',
    ],
    [BIN],
  ])

  const ui = await $.ui.mount(PANE)

  expect(await ui.find({ type: 'Text', text: /\s┋$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /\sbasic$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /\son$/ })).toBeDefined()

  // ┋ is the last separator, so the next one is the first.
  await ui.press({ key: 'sep' })
  expect(argv.at(-1)).toEqual([
    BIN,
    '--themes',
    '[statusline]\nprofile = basic\n[statusline]\nglyphs = on\n',
  ])

  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} tokyonight sep=│ profile=basic glyphs=on`])
  await ui.unmount()
})

test('the Nerd Font arrow previews as itself and goes to the skill by code point', async ($, on) => {
  const argv = engine(on)
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)

  const ui = await $.ui.mount(PANE)

  await ui.press({ key: 'sep' })
  await ui.press({ key: 'sep' })
  expect(argv.at(-1)).toEqual([BIN, '--themes', '[statusline]\nseparator = " \uE0B1 "\n' + BASIC])
  expect(await ui.find({ type: 'Text', text: /\s\uE0B1$/ })).toBeDefined()

  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} tokyonight sep=U+E0B1 profile=basic`])
  await ui.unmount()
})

test('a config holding Nerd Font glyphs opens at them and names them', async ($, on) => {
  const argv = engine(on, {
    config:
      '[statusline]\ntheme = tokyonight\nseparator = " \uE0B1 "\nprofile = detailed\nglyphs = nerdfont\n',
  })
  const runs = skill(on)

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)

  expect(argv).toEqual([
    [
      BIN,
      '--themes',
      '[statusline]\nseparator = " \uE0B1 "\n[statusline]\nprofile = detailed\n[statusline]\nglyphs = nerdfont\n',
    ],
    [BIN],
  ])

  const ui = await $.ui.mount(PANE)

  expect(await ui.find({ type: 'Text', text: /\s\uE0B1$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /\snerdfont$/ })).toBeDefined()
  expect(await ui.find({ type: 'Text', text: /in use: tokyonight/ })).toBeDefined()

  await ui.press({ key: 'apply' })
  expect(runs).toEqual([`/${SKILL} tokyonight sep=U+E0B1 profile=detailed glyphs=nerdfont`])
  await ui.unmount()
})

test('a command that names a preset goes straight to the skill', async ($, on) => {
  const runs = skill(on)

  await $.command.run({ ...TYPED, args: 'nord' })
  expect(runs).toEqual([`/${SKILL} nord`])
})

test('a status line that is not dccstatusline leaves the command to the skill', async ($, on) => {
  const argv = engine(on, { command: 'bash ~/bin/line.sh' })
  const runs = skill(on)

  await $.command.run(TYPED)
  expect(argv).toEqual([])
  expect(runs).toEqual([`/${SKILL}`])
})

test('a picker that is not placed closes, and leaves the command to the skill', async ($, on) => {
  engine(on)
  const runs = skill(on)
  let closed = false

  on('ui.open', () => ({ value: { isPlaced: false, reason: 'too narrow' } }))
  on('ui.close', () => {
    closed = true

    return { value: undefined }
  })

  await $.command.run(TYPED)
  expect(closed).toBe(true)
  expect(runs).toEqual([`/${SKILL}`])
})

test('a binary that predates themes lists none', () => {
  expect(parse('\x1b[0;90mdccstatusline v0.0.4 (build #12)\x1b[0m\n')).toEqual([])
})

test('a preset is named in one word, so a two-word row is none', () => {
  expect(parse('no theme    \x1b[0;94ma\x1b[0m\nnord        \x1b[0;90mb\x1b[0m\n')).toEqual([
    { name: 'nord', spans: [{ text: 'b', fg: 'blackBright' }] },
  ])
})

test('the stock line is one line with no name before it', () => {
  expect(parseStock(STOCK)).toEqual([
    { text: '~/src/acme', fg: 'blueBright' },
    { text: ' │ ', fg: 'blackBright' },
    { text: '42%', fg: 'cyanBright' },
  ])
})

// The status line's own run, kept off the user's config and out of any
// repository, on --themes' sample.
test('the stock line is a plain run on no config, from /', async ($, on) => {
  const inits: (ProcessRunInit | undefined)[] = []
  const argv = engine(on, { inits })

  on('ui.open', () => ({ value: { isPlaced: true } }))
  on('ui.close', () => ({ value: undefined }))
  await $.command.run(TYPED)
  expect(argv.at(-1)).toEqual([BIN])
  expect(inits.at(-1)?.cwd).toBe('/')
  expect(inits.at(-1)?.env).toEqual({ DCCSTATUSLINE_CONFIG: '/dev/null' })
  expect(JSON.parse(inits.at(-1)?.stdin ?? '')).toMatchObject({ cwd: '~/src/acme' })
})

// --themes' resets are 9000 and 302400 seconds past its clock: 02:30 and
// 03:12:00. Thirty seconds more keeps them there while the run takes less.
test('the stock payload is the sample, its resets 30 seconds past the clock', () => {
  expect(JSON.parse(stockPayload(1000))).toEqual({
    cwd: '~/src/acme',
    model: { id: 'claude-opus-5-5', display_name: 'Opus' },
    effort: { level: 'high' },
    context_window: { total_input_tokens: 87333, context_window_size: 200000 },
    rate_limits: {
      five_hour: { used_percentage: 24, resets_at: 10030 },
      seven_day: { used_percentage: 41, resets_at: 303430 },
    },
  })
})

test('each sequence starts over, in 16 colors or 24-bit, fg and bg', () => {
  expect(parse('x  \x1b[0;91;48;2;255;0;0ma\x1b[0;38;2;128;128;128mb\x1b[0m\n')).toEqual([
    {
      name: 'x',
      spans: [
        { text: 'a', fg: 'redBright', bg: '#ff0000' },
        { text: 'b', fg: '#808080' },
      ],
    },
  ])
})
