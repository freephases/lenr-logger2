int getVersionMajorRelease()
{
  return 2;
}
int getVersionMinorRelease()
{
  return 0;
}
int getVersionMajorBuild()
{
  return 0;
}
int getVersionMinorBuild()
{
  return 1;
}

String getVersion()
{
  return String(getVersionMajorRelease())+'.'+String(getVersionMinorRelease())+'.'+String(getVersionMajorBuild())+'.'+String(getVersionMinorBuild());
}
