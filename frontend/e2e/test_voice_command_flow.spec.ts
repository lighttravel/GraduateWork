import { expect, test } from '@playwright/test';

test('voice command dashboard renders core flow', async ({ page }) => {
  await page.goto('/dashboard');

  await expect(page.getByRole('heading', { name: 'Aromatherapy Command Console' })).toBeVisible();
  await expect(page.getByRole('button', { name: /press and hold to capture voice command/i })).toBeVisible();
  await expect(page.getByRole('button', { name: 'Execute Command' })).toBeVisible();
  await expect(page.getByRole('heading', { name: 'Command Pipeline' })).toBeVisible();
});
