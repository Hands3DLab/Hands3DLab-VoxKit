#!/usr/bin/env node
'use strict';

// Build-time resource wrapper. This is tamper resistance, not a secret store:
// the runtime must be able to decrypt the resource on the user's machine.
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const platform = process.argv[2] || process.platform;
const root = path.resolve(__dirname, '..');
const input = path.join(root, 'config.toml');
const outputDir = path.join(root, '.protected');
const output = path.join(outputDir, 'voxkit-config.enc.json');
const version = require(path.join(root, 'package.json')).version;
const keyMaterial = process.env.VOXKIT_RESOURCE_KEY || `Hands3DLab-VoxKit/resource/${platform}/${version}`;
const key = crypto.createHash('sha256').update(keyMaterial).digest();
const iv = crypto.randomBytes(12);
const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
const plaintext = fs.readFileSync(input);
const ciphertext = Buffer.concat([cipher.update(plaintext), cipher.final()]);
const envelope = {
  format: 1,
  algorithm: 'aes-256-gcm',
  platform,
  version,
  iv: iv.toString('base64'),
  tag: cipher.getAuthTag().toString('base64'),
  data: ciphertext.toString('base64')
};
fs.mkdirSync(outputDir, { recursive: true });
fs.writeFileSync(output, `${JSON.stringify(envelope)}\n`);
console.log(`Protected config generated for ${platform}: ${path.relative(process.cwd(), output)}`);
