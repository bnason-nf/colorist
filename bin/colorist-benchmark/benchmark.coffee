fs = require 'fs'
path = require 'path'
{spawnSync} = require 'child_process'

ATTEMPTS = 40
TMPDIR = './benchmark.tmp'

CONFIGS = [
  { name: 'PNG', ext: 'png', args: [] }

  { name: 'JPEG-Q10',  ext: 'jpg', args: ['-q',  '10'] }
  { name: 'JPEG-Q50',  ext: 'jpg', args: ['-q',  '50'] }
  { name: 'JPEG-Q90',  ext: 'jpg', args: ['-q',  '90'] }
  { name: 'JPEG-Q100', ext: 'jpg', args: ['-q', '100'] }

  { name: 'AVIF-8bpc-Q10',  ext: 'avif', args: ['-b', '8', '-q',  '10'] }
  { name: 'AVIF-8bpc-Q50',  ext: 'avif', args: ['-b', '8', '-q',  '50'] }
  { name: 'AVIF-8bpc-Q90',  ext: 'avif', args: ['-b', '8', '-q',  '90'] }
  { name: 'AVIF-8bpc-Q100', ext: 'avif', args: ['-b', '8', '-q', '100'] }

  { name: 'AVIF-10bpc-Q10',  ext: 'avif', args: ['-b', '10', '-q',  '10'] }
  { name: 'AVIF-10bpc-Q50',  ext: 'avif', args: ['-b', '10', '-q',  '50'] }
  { name: 'AVIF-10bpc-Q90',  ext: 'avif', args: ['-b', '10', '-q',  '90'] }
  { name: 'AVIF-10bpc-Q100', ext: 'avif', args: ['-b', '10', '-q', '100'] }

  { name: 'JP2-8bpc-R50',  ext: 'jp2', args: ['-b', '8', '-r',  '50'] }
  { name: 'JP2-8bpc-R100', ext: 'jp2', args: ['-b', '8', '-r', '100'] }
  { name: 'JP2-8bpc-R200', ext: 'jp2', args: ['-b', '8', '-r', '200'] }

  { name: 'JP2-10bpc-R50',  ext: 'jp2', args: ['-b', '10', '-r',  '50'] }
  { name: 'JP2-10bpc-R100', ext: 'jp2', args: ['-b', '10', '-r', '100'] }
  { name: 'JP2-10bpc-R200', ext: 'jp2', args: ['-b', '10', '-r', '200'] }

  { name: 'WebP-Q10',  ext: 'webp', args: ['-q',  '10'] }
  { name: 'WebP-Q50',  ext: 'webp', args: ['-q',  '50'] }
  { name: 'WebP-Q90',  ext: 'webp', args: ['-q',  '90'] }
  { name: 'WebP-Q100', ext: 'webp', args: ['-q', '100'] }
]

benchmark = (filename) ->
  ret = spawnSync("colorist-benchmark", [filename])
  if ret.error
    return null
  try
    results = JSON.parse(ret.stdout)
  catch
    return null
  return results

main = ->
  args = process.argv.slice(2)
  inputDir = args.shift()
  outputCSV = args.shift()
  if not inputDir or not outputCSV
    console.log "Syntax: benchmark.coffee [inputDir] [outputCSV]"
    return

  try
    fs.mkdirSync(TMPDIR)
  catch
    # probably already exists

  outputs = []

  srcFiles = fs.readdirSync(inputDir)
  for srcFile, srcIndex in srcFiles
    if srcFile.match(/^\./)
      # skip "hidden" files
      continue

    srcFilename = "#{inputDir}/#{srcFile}"
    srcParsed = path.parse(srcFilename)
    srcInfo = benchmark(srcFilename)
    console.log "\n[#{srcIndex+1}/#{srcFiles.length}] Benchmarking #{srcFilename} (#{srcInfo.width}x#{srcInfo.height} #{srcInfo.depth}bpc)"

    for config, configIndex in CONFIGS
      console.log " * [#{configIndex+1}/#{CONFIGS.length}] Config: #{config.name}"

      tmpFilename = "#{TMPDIR}/#{srcParsed.name}.#{config.name}.#{config.ext}"
      console.log "   * Generating       : #{tmpFilename}"
      coloristArgs = [ "convert", srcFilename, tmpFilename]
      for extraArg in config.args
        coloristArgs.push extraArg
      # console.log coloristArgs

      try
        fs.unlinkSync(tmpFilename)
      catch
        # probably doesnt exist
      spawnSync("colorist", coloristArgs) # , { stdio: 'inherit' })
      if not fs.existsSync(tmpFilename)
        console.error "Failed to generate: #{tmpFilename}"
        process.exit(-1)

      # convert back for dssim
      dssimFilename = "#{tmpFilename}.dssim.png"
      console.log "   * Calculating DSSIM: #{dssimFilename}"
      try
        fs.unlinkSync(dssimFilename)
      catch
        # probably doesnt exist
      spawnSync("colorist", ['convert', tmpFilename, dssimFilename])
      if not fs.existsSync(dssimFilename)
        console.error "Failed to generate: #{dssimFilename}"
        process.exit(-1)

      # Calc DSSIM using: https://github.com/kornelski/dssim
      dssimProcess = spawnSync("dssim", [ srcFilename, dssimFilename ])
      dssim = -1
      if matches = String(dssimProcess.stdout).match(/^([^ \t]+)/)
        dssim = matches[1]
      console.log "     -> DSSIM      : #{dssim}"

      tmpInfo = benchmark(tmpFilename)
      output =
        name: srcParsed.name
        config: config.name
        width: tmpInfo.width
        height: tmpInfo.height
        depth: tmpInfo.depth
        size: tmpInfo.size
        dssim: dssim
        elapsed: 0

      console.log "   * Decoding #{ATTEMPTS}x, please wait..."
      totalElapsed = 0.0
      for attempt in [0...ATTEMPTS]
        info = benchmark(tmpFilename)
        totalElapsed += info.elapsed
      avgElapsed = totalElapsed / ATTEMPTS
      console.log "     -> Avg elapsed: #{(avgElapsed * 1000).toFixed(2)} ms\n"
      output.elapsed = avgElapsed
      outputs.push output

  csv = ""

  # CSV header
  csv += "\"Name\",\"Config\",\"Elapsed\",\"DSSIM\",\"Size\",\"Width\",\"Height\",\"Depth\"\n"
  for output in outputs
    csv += "\"#{output.name}\",\"#{output.config}\",\"#{(output.elapsed * 1000).toFixed(2)}\",\"#{output.dssim}\",\"#{output.size}\",\"#{output.width}\",\"#{output.height}\",\"#{output.depth}\"\n"
  fs.writeFileSync(outputCSV, csv)
  console.log "Wrote: #{outputCSV}"

main()
